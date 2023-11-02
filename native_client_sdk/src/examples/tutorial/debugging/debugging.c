/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @file debugging.c
 * This example, is a modified version of hello world.  It will start a second
 * thread and cause that thread to crash via a NULL dereference.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"

#include <pthread.h>

#include "error_handling/error_handling.h"

PPB_Messaging* ppb_messaging_interface = NULL;
PPB_Var* ppb_var_interface = NULL;
PPB_Core* ppb_core_interface = NULL;

pthread_t g_NexeThread;
pthread_t g_PPAPIThread;
PP_Instance g_Instance;

volatile int g_CrashTime = 0;

void PostMessage(const char* str);

void layer5(int x, int y) {
  if (g_CrashTime) {
    *(volatile int*)x = y;
  }
}

void layer4(int x) { layer5(x, 1); }

void layer3(int a, int b, int c) { layer4(a + b + c); }

void layer2(int i, int j) { layer3(i, j, 7); }

void layer1(int s, int t) {
  int* junk = (int*)alloca(sizeof(int) * 1234);
  junk[0] = s + 5;
  layer2(junk[0], t + 1);
}

void* NexeMain(void* data) {
  PostMessage("Running Boom thread.");
  while (1) {
    layer1(2, 9);
  }
  return NULL;
}

void PostMessage(const char* str) {
  if (NULL == str)
    return;
  if (NULL == ppb_messaging_interface)
    return;
  if (0 == g_Instance)
    return;

  fprintf(stdout, "%s\n", str);
  fflush(stdout);

  if (ppb_var_interface != NULL) {
    struct PP_Var var = ppb_var_interface->VarFromUtf8(str, strlen(str));
    ppb_messaging_interface->PostMessage(g_Instance, var);
    ppb_var_interface->Release(var);
  }
}

void DumpJson(const char* json) {
  const char kTrcPrefix[] = "TRC: ";
  size_t size = sizeof(kTrcPrefix) + strlen(json) + 1;  // +1 for NULL.
  char* out = (char*)malloc(size);
  strcpy(out, kTrcPrefix);
  strcat(out, json);

  PostMessage(out);
  free(out);
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_Instance = instance;
  g_PPAPIThread = pthread_self();

  PostMessage("LOG: DidCreate");

  /* Request exception callbacks with JSON. */
  EHRequestExceptionsJson(DumpJson);

  /* Report back if the request was honored. */
  if (!EHHanderInstalled()) {
    PostMessage("LOG: Stack traces not available, so don't expect them.\n");
  } else {
    PostMessage("LOG: Stack traces are on.");
  }
  pthread_create(&g_NexeThread, NULL, NexeMain, NULL);
  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  return PP_FALSE;
}

/**
 * Handles message from JavaScript.
 *
 * Any message from JS is a request to cause the main thread to crash.
 */
static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  PostMessage("LOG: Got BOOM");
  g_CrashTime = 1;
}

PP_EXPORT int32_t
PPP_InitializeModule(PP_Module a_module_id, PPB_GetInterface get_browser) {
  ppb_messaging_interface =
      (PPB_Messaging*)(get_browser(PPB_MESSAGING_INTERFACE));
  ppb_var_interface = (PPB_Var*)(get_browser(PPB_VAR_INTERFACE));
  ppb_core_interface = (PPB_Core*)(get_browser(PPB_CORE_INTERFACE));
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
  }
  if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0) {
    static PPP_Messaging messaging_interface = {
      &Messaging_HandleMessage,
    };
    return &messaging_interface;
  }
  return NULL;
}

PP_EXPORT void PPP_ShutdownModule() {}
