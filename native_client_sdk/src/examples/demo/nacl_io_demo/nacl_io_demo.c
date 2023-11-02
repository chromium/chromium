/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io_demo.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/nacl_io.h"

#include "handlers.h"
#include "queue.h"

#if defined(WIN32)
#define va_copy(d, s) ((d) = (s))
#endif

/**
 * The location of MAX is inconsitantly between LIBCs, so instead
 * we define it here for consistency.
 */
static int larger_int_of(int a, int b) {
  if (a > b)
    return a;
  return b;
}

typedef struct {
  const char* name;
  HandleFunc function;
} FuncNameMapping;

static PP_Instance g_instance = 0;
static PPB_GetInterface g_get_browser_interface = NULL;
static PPB_Messaging* g_ppb_messaging = NULL;
PPB_Var* g_ppb_var = NULL;
PPB_VarArray* g_ppb_var_array = NULL;
PPB_VarDictionary* g_ppb_var_dictionary = NULL;

static FuncNameMapping g_function_map[] = {
    {"fopen", HandleFopen},
    {"fwrite", HandleFwrite},
    {"fread", HandleFread},
    {"fseek", HandleFseek},
    {"fclose", HandleFclose},
    {"fflush", HandleFflush},
    {"stat", HandleStat},
    {"opendir", HandleOpendir},
    {"readdir", HandleReaddir},
    {"closedir", HandleClosedir},
    {"mkdir", HandleMkdir},
    {"rmdir", HandleRmdir},
    {"chdir", HandleChdir},
    {"getcwd", HandleGetcwd},
    {"getaddrinfo", HandleGetaddrinfo},
    {"gethostbyname", HandleGethostbyname},
    {"connect", HandleConnect},
    {"send", HandleSend},
    {"recv", HandleRecv},
    {"close", HandleClose},
    {NULL, NULL},
};

/** A handle to the thread the handles messages. */
static pthread_t g_handle_message_thread;
static pthread_t g_echo_thread;

/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|.
 */
struct PP_Var CStrToVar(const char* str) {
  return g_ppb_var->VarFromUtf8(str, strlen(str));
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A printf format string.
 * @param[in] args The printf arguments.
 * @return The newly constructed string. Caller takes ownership. */
char* VprintfToNewString(const char* format, va_list args) {
  va_list args_copy;
  int length;
  char* buffer;
  int result;

  va_copy(args_copy, args);
  length = vsnprintf(NULL, 0, format, args);
  buffer = (char*)malloc(length + 1); /* +1 for NULL-terminator. */
  result = vsnprintf(&buffer[0], length + 1, format, args_copy);
  if (result != length) {
    assert(0);
    return NULL;
  }
  return buffer;
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A print format string.
 * @param[in] ... The printf arguments.
 * @return The newly constructed string. Caller takes ownership.
 */
char* PrintfToNewString(const char* format, ...) {
  va_list args;
  char* result;
  va_start(args, format);
  result = VprintfToNewString(format, args);
  va_end(args);
  return result;
}

/**
 * Vprintf to a new PP_Var.
 * @param[in] format A print format string.
 * @param[in] va_list The printf arguments.
 * @return A new PP_Var.
 */
static struct PP_Var VprintfToVar(const char* format, va_list args) {
  struct PP_Var var;
  char* string = VprintfToNewString(format, args);
  var = g_ppb_var->VarFromUtf8(string, strlen(string));
  free(string);
  return var;
}

/**
 * Convert a PP_Var to a C string.
 * @param[in] var The PP_Var to convert.
 * @return A newly allocated, NULL-terminated string.
 */
static const char* VarToCStr(struct PP_Var var) {
  uint32_t length;
  const char* str = g_ppb_var->VarToUtf8(var, &length);
  if (str == NULL) {
    return NULL;
  }

  /* str is NOT NULL-terminated. Copy using memcpy. */
  char* new_str = (char*)malloc(length + 1);
  memcpy(new_str, str, length);
  new_str[length] = 0;
  return new_str;
}

/**
 * Get a value from a Dictionary, given a string key.
 * @param[in] dict The dictionary to look in.
 * @param[in] key The key to look up.
 * @return PP_Var The value at |key| in the |dict|. If the key doesn't exist,
 *     return a PP_Var with the undefined value.
 */
struct PP_Var GetDictVar(struct PP_Var dict, const char* key) {
  struct PP_Var key_var = CStrToVar(key);
  struct PP_Var value = g_ppb_var_dictionary->Get(dict, key_var);
  g_ppb_var->Release(key_var);
  return value;
}

/**
 * Post a message to JavaScript.
 * @param[in] format A printf format string.
 * @param[in] ... The printf arguments.
 */
static void PostMessage(const char* format, ...) {
  struct PP_Var var;
  va_list args;

  va_start(args, format);
  var = VprintfToVar(format, args);
  va_end(args);

  g_ppb_messaging->PostMessage(g_instance, var);
  g_ppb_var->Release(var);
}

/**
 * Given a message from JavaScript, parse it for functions and parameters.
 *
 * The format of the message is:
 * {
 *  "cmd": <function name>,
 *  "args": [<arg0>, <arg1>, ...]
 * }
 *
 * @param[in] message The message to parse.
 * @param[out] out_function The function name.
 * @param[out] out_params A PP_Var array.
 * @return 0 if successful, otherwise 1.
 */
static int ParseMessage(struct PP_Var message,
                        const char** out_function,
                        struct PP_Var* out_params) {
  if (message.type != PP_VARTYPE_DICTIONARY) {
    return 1;
  }

  struct PP_Var cmd_value = GetDictVar(message, "cmd");
  *out_function = VarToCStr(cmd_value);
  g_ppb_var->Release(cmd_value);
  if (cmd_value.type != PP_VARTYPE_STRING) {
    return 1;
  }

  *out_params = GetDictVar(message, "args");
  if (out_params->type != PP_VARTYPE_ARRAY) {
    return 1;
  }

  return 0;
}

/**
 * Given a function name, look up its handler function.
 * @param[in] function_name The function name to look up.
 * @return The handler function mapped to |function_name|.
 */
static HandleFunc GetFunctionByName(const char* function_name) {
  FuncNameMapping* map_iter = g_function_map;
  for (; map_iter->name; ++map_iter) {
    if (strcmp(map_iter->name, function_name) == 0) {
      return map_iter->function;
    }
  }

  return NULL;
}

/**
 * Handle as message from JavaScript on the worker thread.
 *
 * @param[in] message The message to parse and handle.
 */
static void HandleMessage(struct PP_Var message) {
  const char* function_name;
  struct PP_Var params;
  if (ParseMessage(message, &function_name, &params)) {
    PostMessage("Error: Unable to parse message");
    return;
  }

  HandleFunc function = GetFunctionByName(function_name);
  if (!function) {
    /* Function name wasn't found. Error. */
    PostMessage("Error: Unknown function \"%s\"", function_name);
    return;
  }

  /* Function name was found, call it. */
  struct PP_Var result_var;
  const char* error;
  int result = (*function)(params, &result_var, &error);
  if (result != 0) {
    /* Error. */
    if (error != NULL) {
      PostMessage("Error: \"%s\" failed: %s.", function_name, error);
      free((void*)error);
    } else {
      PostMessage("Error: \"%s\" failed.", function_name);
    }
    return;
  }

  /* Function returned an output dictionary. Send it to JavaScript. */
  g_ppb_messaging->PostMessage(g_instance, result_var);
  g_ppb_var->Release(result_var);
}


/**
 * Helper function used by EchoThread which reads from a file descriptor
 * and writes all the data that it reads back to the same descriptor.
 */
static void EchoInput(int fd) {
  char buffer[512];
  while (1) {
    int rtn = read(fd, buffer, 512);
    if (rtn > 0) {
      int wrote = write(fd, buffer, rtn);
      if (wrote < rtn)
        PostMessage("only wrote %d/%d bytes\n", wrote, rtn);
    } else {
      if (rtn < 0 && errno != EAGAIN)
        PostMessage("read failed: %d (%s)\n", errno, strerror(errno));
      break;
    }
  }
}

/**
 * Worker thread that listens for input on JS pipe nodes and echos all input
 * back to the same pipe.
 */
static void* EchoThread(void* user_data) {
  int fd1 = open("/dev/jspipe1", O_RDWR | O_NONBLOCK);
  int fd2 = open("/dev/jspipe2", O_RDWR | O_NONBLOCK);
  int fd3 = open("/dev/jspipe3", O_RDWR | O_NONBLOCK);
  int nfds = larger_int_of(fd1, fd2);
  nfds = larger_int_of(nfds, fd3);
  while (1) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd1, &readfds);
    FD_SET(fd2, &readfds);
    FD_SET(fd3, &readfds);
    int rtn = select(nfds + 1, &readfds, NULL, NULL, NULL);
    if (rtn < 0 && errno != EAGAIN) {
      PostMessage("select failed: %s\n", strerror(errno));
      break;
    }
    if (rtn > 0) {
      if (FD_ISSET(fd1, &readfds))
        EchoInput(fd1);
      if (FD_ISSET(fd2, &readfds))
        EchoInput(fd2);
      if (FD_ISSET(fd3, &readfds))
        EchoInput(fd3);
    }

  }
  close(fd1);
  close(fd2);
  close(fd3);
  return 0;
}

/**
 * A worker thread that handles messages from JavaScript.
 * @param[in] user_data Unused.
 * @return unused.
 */
void* HandleMessageThread(void* user_data) {
  while (1) {
    struct PP_Var message = DequeueMessage();
    HandleMessage(message);
    g_ppb_var->Release(message);
  }
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_instance = instance;
  nacl_io_init_ppapi(instance, g_get_browser_interface);

  // By default, nacl_io mounts / to pass through to the original NaCl
  // filesystem (which doesn't do much). Let's remount it to a memfs
  // filesystem.
  umount("/");
  mount("", "/", "memfs", 0, "");

  mount("",                                       /* source */
        "/persistent",                            /* target */
        "html5fs",                                /* filesystemtype */
        0,                                        /* mountflags */
        "type=PERSISTENT,expected_size=1048576"); /* data */

  mount("",       /* source. Use relative URL */
        "/http",  /* target */
        "httpfs", /* filesystemtype */
        0,        /* mountflags */
        "");      /* data */

  pthread_create(&g_handle_message_thread, NULL, &HandleMessageThread, NULL);
  pthread_create(&g_echo_thread, NULL, &EchoThread, NULL);
  InitializeMessageQueue();

  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {
}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  /* NaCl modules do not need to handle the document load function. */
  return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  /* Special case for jspipe input handling */
  if (message.type != PP_VARTYPE_DICTIONARY) {
    PostMessage("Got unexpected message type: %d\n", message.type);
    return;
  }

  struct PP_Var pipe_var = CStrToVar("pipe");
  struct PP_Var pipe_name = g_ppb_var_dictionary->Get(message, pipe_var);
  g_ppb_var->Release(pipe_var);

  /* Special case for jspipe input handling */
  if (pipe_name.type == PP_VARTYPE_STRING) {
    char file_name[PATH_MAX];
    snprintf(file_name, PATH_MAX, "/dev/%s", VarToCStr(pipe_name));
    int fd = open(file_name, O_RDONLY);
    g_ppb_var->Release(pipe_name);
    if (fd < 0) {
      PostMessage("Warning: opening %s failed.", file_name);
      goto done;
    }
    if (ioctl(fd, NACL_IOC_HANDLEMESSAGE, &message) != 0) {
      PostMessage("Error: ioctl on %s failed: %s", file_name, strerror(errno));
    }
    close(fd);
    goto done;
  }

  g_ppb_var->AddRef(message);
  if (!EnqueueMessage(message)) {
    g_ppb_var->Release(message);
    PostMessage("Warning: dropped message because the queue was full.");
  }

done:
  g_ppb_var->Release(pipe_name);
}

#define GET_INTERFACE(var, type, name)            \
  var = (type*)(get_browser(name));               \
  if (!var) {                                     \
    printf("Unable to get interface " name "\n"); \
    return PP_ERROR_FAILED;                       \
  }

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
  g_get_browser_interface = get_browser;
  GET_INTERFACE(g_ppb_messaging, PPB_Messaging, PPB_MESSAGING_INTERFACE);
  GET_INTERFACE(g_ppb_var, PPB_Var, PPB_VAR_INTERFACE);
  GET_INTERFACE(g_ppb_var_array, PPB_VarArray, PPB_VAR_ARRAY_INTERFACE);
  GET_INTERFACE(
      g_ppb_var_dictionary, PPB_VarDictionary, PPB_VAR_DICTIONARY_INTERFACE);
  return PP_OK;
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    static PPP_Instance instance_interface = {
        &Instance_DidCreate,
        &Instance_DidDestroy,
        &Instance_DidChangeView,
        &Instance_DidChangeFocus,
        &Instance_HandleDocumentLoad,
    };
    return &instance_interface;
  } else if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0) {
    static PPP_Messaging messaging_interface = {
        &Messaging_HandleMessage,
    };
    return &messaging_interface;
  }
  return NULL;
}

PP_EXPORT void PPP_ShutdownModule() {
}
