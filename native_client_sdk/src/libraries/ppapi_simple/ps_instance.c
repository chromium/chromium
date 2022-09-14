/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "ppapi_simple/ps_instance.h"

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>  /* Needed for struct winsize in bionic */
#include <unistd.h>

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_module.h>
#include <ppapi/c/pp_rect.h>
#include <ppapi/c/pp_size.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppp.h>
#include <ppapi/c/ppp_graphics_3d.h>
#include <ppapi/c/ppp_input_event.h>
#include <ppapi/c/ppp_instance.h>
#include <ppapi/c/ppp_messaging.h>
#include <ppapi/c/ppp_mouse_lock.h>

#include "nacl_io/ioctl.h"
#include "nacl_io/nacl_io.h"
#include "nacl_io/log.h"
#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_internal.h"
#include "ppapi_simple/ps_main.h"

struct StartInfo {
  uint32_t argc_;
  char** argv_;
};

PP_Instance g_ps_instance;
PPB_GetInterface g_ps_get_interface;
PSMainFunc_t g_ps_main_cb;

static enum PSVerbosity s_verbosity;

/* TTY handling */
static int s_tty_fd;
static const char* s_tty_prefix;

/* Condition variable and lock used to wait for exit confirmation from
 * JavaScript. */
static pthread_cond_t s_exit_cond;
static pthread_mutex_t s_exit_lock;

/* A message to Post to JavaScript instead of exiting, or NULL if exit() should
 * be called instead. */
static char* s_exit_message;

static int ProcessProperties(void);
ssize_t TtyOutputHandler(const char* buf, size_t count, void* user_data);
static void MessageHandlerExit(struct PP_Var key,
                               struct PP_Var value,
                               void* user_data);
static void MessageHandlerInput(struct PP_Var key,
                                struct PP_Var value,
                                void* user_data);
static void MessageHandlerResize(struct PP_Var key,
                                 struct PP_Var value,
                                 void* user_data);
static void HandleResize(int width, int height);
static void* MainThread(void* info);
static void ExitHandshake(int status, void* user_data);

static void PostMessageString(const char* message) {
  struct PP_Var message_var =
      PSInterfaceVar()->VarFromUtf8(message, strlen(message));
  PSInterfaceMessaging()->PostMessage(g_ps_instance, message_var);
  PSInterfaceVar()->Release(message_var);
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_ps_instance = instance;
  g_ps_main_cb = PSUserMainGet();
  s_verbosity = PSV_LOG;
  PSInterfaceInputEvent()->RequestInputEvents(
      g_ps_instance, PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD |
                      PP_INPUTEVENT_CLASS_WHEEL | PP_INPUTEVENT_CLASS_TOUCH);

  uint32_t i;
  struct StartInfo* si = malloc(sizeof(struct StartInfo));

  si->argc_ = 0;
  si->argv_ = calloc(argc + 1, sizeof(char*));
  si->argv_[0] = NULL;

  /* Process embed attributes into the environment.
   * Convert the attribute names to uppercase as environment variables are case
   * sensitive but are almost universally uppercase in practice. */
  for (i = 0; i < argc; i++) {
    char* key = strdup(argn[i]);
    char* c = key;
    while (*c) {
      *c = toupper((int)*c);
      ++c;
    }
    setenv(key, argv[i], 1);
    free(key);
  }

  /* Set a default value for SRC. */
  setenv("SRC", "NMF?", 0);
  /* Use the src tag name if ARG0 is not explicitly specified. */
  setenv("ARG0", getenv("SRC"), 0);

  /* Walk ARG0..ARGn populating argv until an argument is missing. */
  for (;;) {
    char arg_name[32];
    snprintf(arg_name, 32, "ARG%d", si->argc_);
    const char* next_arg = getenv(arg_name);
    if (NULL == next_arg)
      break;

    si->argv_[si->argc_++] = strdup(next_arg);
  }

  int props_processed = ProcessProperties();

  /* Log arg values only once ProcessProperties has been called so that the
   * PS_VERBOSITY attribute will be in effect. */
  for (i = 0; i < argc; i++) {
    if (argv[i]) {
      PSInstanceTrace("attribs[%d] '%s=%s'\n", i, argn[i], argv[i]);
    } else {
      PSInstanceTrace("attribs[%d] '%s'\n", i, argn[i]);
    }
  }

  for (i = 0; i < si->argc_; i++) {
    PSInstanceTrace("argv[%d] '%s'\n", i, si->argv_[i]);
  }

  if (!props_processed) {
    PSInstanceWarn("Skipping create thread.\n");
    return PP_FALSE;
  }

  pthread_t main_thread;
  int ret = pthread_create(&main_thread, NULL, MainThread, si);
  PSInstanceTrace("Created thread: %d.\n", ret);
  return ret == 0 ? PP_TRUE : PP_FALSE;
}

int ProcessProperties(void) {
  /* Reset verbosity if passed in */
  const char* verbosity = getenv("PS_VERBOSITY");
  if (verbosity)
    PSInstanceSetVerbosity(atoi(verbosity));

  /* Enable NaCl IO to map STDIN, STDOUT, and STDERR */
  nacl_io_init_ppapi(PSGetInstanceId(), PSGetInterface);

  s_tty_prefix = getenv("PS_TTY_PREFIX");
  if (s_tty_prefix) {
    s_tty_fd = open("/dev/tty", O_WRONLY);
    if (s_tty_fd >= 0) {
      PSEventRegisterMessageHandler(s_tty_prefix, MessageHandlerInput, NULL);
      const char* tty_resize = getenv("PS_TTY_RESIZE");
      if (tty_resize)
        PSEventRegisterMessageHandler(tty_resize, MessageHandlerResize, NULL);

      char* tty_rows = getenv("PS_TTY_ROWS");
      char* tty_cols = getenv("PS_TTY_COLS");
      if (tty_rows && tty_cols) {
        char* end = tty_rows;
        int rows = strtol(tty_rows, &end, 10);
        if (*end != '\0' || rows < 0) {
          PSInstanceError("Invalid value for PS_TTY_ROWS: %s\n", tty_rows);
        } else {
          end = tty_cols;
          int cols = strtol(tty_cols, &end, 10);
          if (*end != '\0' || cols < 0)
            PSInstanceError("Invalid value for PS_TTY_COLS: %s\n", tty_cols);
          else
            HandleResize(cols, rows);
        }
      } else if (tty_rows || tty_cols) {
        PSInstanceError("PS_TTY_ROWS and PS_TTY_COLS must be set together\n");
      }

      struct tioc_nacl_output handler;
      handler.handler = TtyOutputHandler;
      handler.user_data = NULL;
      ioctl(s_tty_fd, TIOCNACLOUTPUT, &handler);
    } else {
      PSInstanceError("Failed to open /dev/tty.\n");
    }
  }

  /* Set default values */
  setenv("PS_STDIN", "/dev/stdin", 0);
  setenv("PS_STDOUT", "/dev/stdout", 0);
  setenv("PS_STDERR", "/dev/console3", 0);

  int fd0 = open(getenv("PS_STDIN"), O_RDONLY);
  dup2(fd0, 0);

  int fd1 = open(getenv("PS_STDOUT"), O_WRONLY);
  dup2(fd1, 1);

  int fd2 = open(getenv("PS_STDERR"), O_WRONLY);
  dup2(fd2, 2);

  PSEventRegisterMessageHandler("jspipe1", MessageHandlerInput, NULL);
  PSEventRegisterMessageHandler("jspipe2", MessageHandlerInput, NULL);
  PSEventRegisterMessageHandler("jspipe3", MessageHandlerInput, NULL);

  s_exit_message = getenv("PS_EXIT_MESSAGE");

  /* If PS_EXIT_MESSAGE is set in the environment then we perform a handshake
   * with JavaScript when program exits. */
  if (s_exit_message != NULL)
    nacl_io_set_exit_callback(ExitHandshake, NULL);

  /* Set line buffering on stdout and stderr */
#if !defined(WIN32)
  setvbuf(stderr, NULL, _IOLBF, 0);
  setvbuf(stdout, NULL, _IOLBF, 0);
#endif
  return 1;
}

ssize_t TtyOutputHandler(const char* data, size_t count, void* user_data) {
  /* We prepend the s_tty_prefix to the data in buf, then package it up and
   * post it as a message to javascript. */
  size_t tty_prefix_len = strlen(s_tty_prefix);
  char* message = alloca(tty_prefix_len + count + 1);
  memcpy(message, s_tty_prefix, tty_prefix_len);
  memcpy(message + tty_prefix_len, data, count);
  message[tty_prefix_len + count] = 0;
  PostMessageString(message);
  return count;
}

void MessageHandlerExit(struct PP_Var key,
                        struct PP_Var value,
                        void* user_data) {
  pthread_mutex_lock(&s_exit_lock);
  pthread_cond_signal(&s_exit_cond);
  pthread_mutex_unlock(&s_exit_lock);
}

void MessageHandlerInput(struct PP_Var key,
                         struct PP_Var value,
                         void* user_data) {
  uint32_t key_len;
  const char* key_str = PSInterfaceVar()->VarToUtf8(key, &key_len);

  const char* filename = NULL;
  if (strncmp(key_str, s_tty_prefix, key_len) == 0) {
    filename = "/dev/tty";
  } else if (strncmp(key_str, "jspipe1", key_len) == 0) {
    filename = "/dev/jspipe1";
  } else if (strncmp(key_str, "jspipe2", key_len) == 0) {
    filename = "/dev/jspipe2";
  } else if (strncmp(key_str, "jspipe3", key_len) == 0) {
    filename = "/dev/jspipe3";
  } else {
    PSInstanceError("unexpected input key: %s", key_str);
    return;
  }

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    PSInstanceError("error opening file: %s (%s)", filename, strerror(errno));
    return;
  }

  int ret = ioctl(fd, NACL_IOC_HANDLEMESSAGE, &value);
  if (ret != 0) {
    PSInstanceError("ioctl on %s failed: %d.\n", filename, ret);
    close(fd);
    return;
  }

  close(fd);
}

void MessageHandlerResize(struct PP_Var key,
                          struct PP_Var value,
                          void* user_data) {
  assert(value.type == PP_VARTYPE_ARRAY);
  assert(PSInterfaceVarArray()->GetLength(value) == 2);

  struct PP_Var width_var = PSInterfaceVarArray()->Get(value, 0);
  struct PP_Var height_var = PSInterfaceVarArray()->Get(value, 1);

  assert(width_var.type == PP_VARTYPE_INT32);
  assert(height_var.type == PP_VARTYPE_INT32);

  int width = width_var.value.as_int;
  int height = height_var.value.as_int;

  HandleResize(width, height);
}

void HandleResize(int width, int height) {
  struct winsize size;
  memset(&size, 0, sizeof(size));
  size.ws_col = width;
  size.ws_row = height;
  ioctl(s_tty_fd, TIOCSWINSZ, &size);
}

void* MainThread(void* info) {
  int ret;
  uint32_t i;
  PSInstanceTrace("Running MainThread.\n");
  struct StartInfo* si = (struct StartInfo*)info;

  PP_Resource message_loop = PSInterfaceMessageLoop()->Create(g_ps_instance);
  if (PSInterfaceMessageLoop()->AttachToCurrentThread(message_loop) != PP_OK) {
    PSInstanceError("Unable to attach message loop to thread.\n");
    return NULL;
  }

  if (!g_ps_main_cb) {
    PSInstanceError("No main defined.\n");
    return 0;
  }

  PSInstanceTrace("Starting MAIN.\n");
  ret = g_ps_main_cb(si->argc_, si->argv_);
  PSInstanceLog("Main thread returned with %d.\n", ret);

  /* Clean up StartInfo. */
  for (i = 0; i < si->argc_; i++) {
    free(si->argv_[i]);
  }
  free(si->argv_);
  free(si);

  /* Exit the entire process once the 'main' thread returns. The error code
   * will be available to javascript via the exitcode parameter of the crash
   * event. */
#ifdef __native_client__
  exit(ret);
#else
  ExitHandshake(ret, NULL);
#endif
  return NULL;
}

void ExitHandshake(int status, void* user_data) {
  if (s_exit_message == NULL)
    return;

  PSEventRegisterMessageHandler(s_exit_message, MessageHandlerExit, NULL);

  /* exit message + ':' + num + \0 */
  size_t message_len = strlen(s_exit_message) + 1 + 11 + 1;
  char* message = alloca(message_len);
  snprintf(message, message_len, "%s:%d", s_exit_message, status);

  pthread_mutex_lock(&s_exit_lock);
  PostMessageString(message);
  pthread_cond_wait(&s_exit_cond, &s_exit_lock);
  pthread_mutex_unlock(&s_exit_lock);
}

static void Instance_DidDestroy(PP_Instance instance) {
}

static void Instance_DidChangeView(PP_Instance instance, PP_Resource view) {
  struct PP_Rect rect;
  if (PSInterfaceView()->GetRect(view, &rect)) {
    PSInstanceLog("Got View change: %d,%d\n", rect.size.width,
                  rect.size.height);
    PSEventPostResource(PSE_INSTANCE_DIDCHANGEVIEW, view);
  }
}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  PSInstanceLog("Got Focus change: %s\n", has_focus ? "FOCUS ON" : "FOCUS OFF");
  PSEventPostBool(PSE_INSTANCE_DIDCHANGEFOCUS, has_focus ? PP_TRUE : PP_FALSE);
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  PSInstanceTrace("Got Message\n");
  PSEventPostVar(PSE_INSTANCE_HANDLEMESSAGE, message);
}

static PP_Bool InputEvent_HandleInputEvent(PP_Instance instance,
                                           PP_Resource input_event) {
  PSEventPostResource(PSE_INSTANCE_HANDLEINPUT, input_event);
  return PP_TRUE;
}

static void MouseLock_MouseLockLost(PP_Instance instance) {
  PSInstanceLog("MouseLockLost\n");
  PSEventPost(PSE_MOUSELOCK_MOUSELOCKLOST);
}

static void Graphics3D_Graphics3DContextLost(PP_Instance instance) {
  PSInstanceLog("Graphics3DContextLost\n");
  PSEventPost(PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST);
}

void PSInstanceSetVerbosity(enum PSVerbosity verbosity) {
  s_verbosity = verbosity;
}

static void VALog(enum PSVerbosity verbosity, const char* fmt, va_list args) {
  if (verbosity <= s_verbosity) {
    fprintf(stderr, "ps: ");
    vfprintf(stderr, fmt, args);
  }
}

void PSInstanceTrace(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_TRACE, fmt, ap);
  va_end(ap);
}

void PSInstanceLog(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_LOG, fmt, ap);
  va_end(ap);
}

void PSInstanceWarn(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_WARN, fmt, ap);
  va_end(ap);
}

void PSInstanceError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  VALog(PSV_ERROR, fmt, ap);
  va_end(ap);
}

const void* PSGetInterfaceImplementation(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE_1_1) == 0) {
    static struct PPP_Instance_1_1 interface = {
        &Instance_DidCreate,
        &Instance_DidDestroy,
        &Instance_DidChangeView,
        &Instance_DidChangeFocus,
        &Instance_HandleDocumentLoad,
    };
    return &interface;
  } else if (strcmp(interface_name, PPP_MESSAGING_INTERFACE_1_0) == 0) {
    static struct PPP_Messaging_1_0 interface = {
        &Messaging_HandleMessage,
    };
    return &interface;
  } else if (strcmp(interface_name, PPP_INPUT_EVENT_INTERFACE_0_1) == 0) {
    static struct PPP_InputEvent_0_1 interface = {
        &InputEvent_HandleInputEvent,
    };
    return &interface;
  } else if (strcmp(interface_name, PPP_MOUSELOCK_INTERFACE_1_0) == 0) {
    static struct PPP_MouseLock_1_0 interface = {
        &MouseLock_MouseLockLost,
    };
    return &interface;
  } else if (strcmp(interface_name, PPP_GRAPHICS_3D_INTERFACE_1_0) == 0) {
    static struct PPP_Graphics3D_1_0 interface = {
        &Graphics3D_Graphics3DContextLost,
    };
    return &interface;
  }

  return NULL;
}
