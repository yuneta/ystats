/****************************************************************************
 *          MAIN_YSTATS.C
 *          ystats main
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <unistd.h>
#include <yuneta.h>
#include "c_ystats.h"
#include "yuno_ystats.h"

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    int print_role;
    int refresh_time;           /* refresh_time time, in seconds (0 remove subscription) */
    char *url;
    char *stats;
    char *attribute;
    char *realm_name;
    char *yuno_role;
    char *yuno_name;
    char *yuno_service;
    char *gobj_name;

    int verbose;                /* verbose */
    int print;
    int print_version;
    int print_yuneta_version;
    int use_config_file;
    const char *config_json_file;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "ystats"
#define APP_DOC         "Yuneta Statistics"

#define APP_VERSION     "4.9.2"
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': 'agent',                                     \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/ystats'                         \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': 'ystats',                                      \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                 \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 32,                                        \n\
        'MEM_MAX_BLOCK': 65536,             #^^ 64*K                \n\
        'MEM_SUPERBLOCK': 131072,           #^^ 128*K               \n\
        'MEM_MAX_SYSTEM_MEMORY': 1048576,   #^^ 1*M                 \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': %d       #^^ 7                   \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'ystats',                                       \n\
            'gclass': 'YStats',                                     \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *      Data
 ***************************************************************************/
// Set by yuneta_entry_point()
// const char *argp_program_version = APP_NAME " " APP_VERSION;
// const char *argp_program_bug_address = APP_SUPPORT;

/* Program documentation. */
static char doc[] = APP_DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{0,                 0,      0,          0,      "Remote Service keys", 1},
{"refresh_time",    't',    "NUMBER",   0,      "Refresh time, in seconds (default 1).", 1},
{"stats",           's',    "STATS",    0,      "Statistic to ask.", 2},
{"attribute",       'a',    "ATTRIBUTE",0,      "Attribute to ask .", 3},
{"gobj_name",       'g',    "GOBJNAME", 0,      "Attribute's GObj (named-gobj or full-path).", 3},
{0,                 0,      0,          0,      "Connection keys", 4},
{"url",             'u',    "URL",      0,      "Url to connect (default 'ws://127.0.0.1:1991').", 4},
{"realm_name",      'n',    "REALNAME", 0,      "Remote realm name.", 4},
{"yuno_role",       'O',    "YUNOROLE", 0,      "Remote yuno role.", 4},
{"yuno_name",       'o',    "YUNONAME", 0,      "Remote yuno name.", 4},
{"service",         'S',    "SERVICE",  0,      "Remote yuno service (default '__default_service__').", 4},
{0,                 0,      0,          0,      "Local keys.", 5},
{"print",           'p',    0,          0,      "Print configuration.", 5},
{"config-file",     'f',    "FILE",     0,      "load settings from json config file or [files]", 5},
{"print-role",      'r',    0,          0,      "print the basic yuno's information"},
{"verbose",         'l',    "LEVEL",    0,      "Verbose level.", 5},
{"version",         'v',    0,          0,      "Print program version.", 5},
{"yuneta-version",  'V',    0,          0,      "Print yuneta version", 5},
{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc
};

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 't':
        if(arg) {
            arguments->refresh_time = atoi(arg);
        }
        break;

    case 'u':
        arguments->url = arg;
        break;

    case 'O':
        arguments->yuno_role = arg;
        break;
    case 'o':
        arguments->yuno_name = arg;
        break;

    case 'S':
        arguments->yuno_service = arg;
        break;
    case 's':
        arguments->stats = arg;
        break;
    case 'n':
        arguments->realm_name = arg;
        break;
    case 'a':
        arguments->attribute = arg;
        break;
    case 'g':
        arguments->gobj_name = arg;
        break;

    case 'v':
        arguments->print_version = 1;
        break;

    case 'V':
        arguments->print_yuneta_version = 1;
        break;

    case 'l':
        if(arg) {
            arguments->verbose = atoi(arg);
        }
        break;

    case 'r':
        arguments->print_role = 1;
        break;
    case 'f':
        arguments->config_json_file = arg;
        arguments->use_config_file = 1;
        break;
    case 'p':
        arguments->print = 1;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *                      Register
 ***************************************************************************/
static void register_yuno_and_more(void)
{
    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_ystats();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_YSTATS);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    struct arguments arguments;
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.refresh_time = 1;
    arguments.url = "ws://127.0.0.1:1991";
    arguments.stats = "";
    arguments.gobj_name = "";
    arguments.attribute = "";
    arguments.realm_name = 0;
    arguments.yuno_role = 0;
    arguments.yuno_name = 0;
    arguments.yuno_service = "__default_service__";

    /*
     *  Save args
     */
    char argv0[512];
    char *argvs[]= {argv0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memset(argv0, 0, sizeof(argv0));
    strncpy(argv0, argv[0], sizeof(argv0)-1);
    int idx = 1;

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*
     *  Check arguments
     */
    if(arguments.print_version) {
        printf("%s %s\n", APP_NAME, APP_VERSION);
        exit(0);
    }
    if(arguments.print_yuneta_version) {
        printf("%s\n", __yuneta_long_version__);
        exit(0);
    }

    /*
     *  Put configuration
     */
    if(arguments.use_config_file) {
        int l = strlen("--config-file=") + strlen(arguments.config_json_file) + 4;
        char *param2 = malloc(l);
        snprintf(param2, l, "--config-file=%s", arguments.config_json_file);
        argvs[idx++] = param2;
    } else {
        json_t *kw_utility = json_pack("{s:{s:i, s:b, s:s, s:s, s:s, s:s, s:s}}",
            "global",
            "YStats.refresh_time", arguments.refresh_time,
            "YStats.verbose", arguments.verbose,
            "YStats.stats", arguments.stats,
            "YStats.gobj_name", arguments.gobj_name,
            "YStats.attribute", arguments.attribute,
            "YStats.url", arguments.url,
            "YStats.yuno_service", arguments.yuno_service
        );
        json_t *jn_ystats = kw_get_dict_value(kw_utility, "global", 0, 0);
        if(arguments.realm_name) {
            json_object_set_new(jn_ystats, "YStats.realm_name", json_string(arguments.realm_name));
        }
        if(arguments.yuno_role) {
            json_object_set_new(jn_ystats, "YStats.yuno_role", json_string(arguments.yuno_role));
        }
        if(arguments.yuno_name) {
            json_object_set_new(jn_ystats, "YStats.yuno_name", json_string(arguments.yuno_name));
        }

        char *param1_ = json_dumps(kw_utility, JSON_COMPACT);
        if(!param1_) {
            printf("Some parameter is wrong\n");
            exit(-1);
        }
        int len = strlen(param1_) + 3;
        char *param1 = malloc(len);
        if(param1) {
            memset(param1, 0, len);
            snprintf(param1, len, "%s", param1_);
        }
        free(param1_);
        argvs[idx++] = param1;
    }

    if(arguments.print) {
        argvs[idx++] = "-P";
    }
    if(arguments.print_role) {
        argvs[idx++] = "--print-role";
    }

    /*------------------------------------------------*
     *  To trace memory
     *------------------------------------------------*/
#ifdef DEBUG
    static uint32_t mem_list[] = {0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

    int log_handler_options = -1;
    if(arguments.verbose == 0) {
        log_handler_options = 7;
    }
    if(arguments.verbose > 0) {
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents2", TRUE);
        gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "kw", TRUE);
    }
    if(arguments.verbose > 1) {
        gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);
    }
    if(arguments.verbose > 2) {
        gobj_set_gobj_trace(0, "machine", TRUE, 0);
        gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
        gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
        gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
        gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    }
    gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

    static char my_variable_config[16*1024];
    snprintf(my_variable_config, sizeof(my_variable_config), variable_config, log_handler_options);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(my_variable_config);
    yuneta_setup(
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    );
    return yuneta_entry_point(
        idx, argvs,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        my_variable_config,
        register_yuno_and_more
    );
}
