// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi_simple/ps_event.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_interface.h"

#define NO_BLOCK 0
#define BLOCK 1

struct PSMessageHandlerInfo {
  char* message_name;
  PSMessageHandler_t func;
  void* user_data;
  struct PSMessageHandlerInfo* prev;
  struct PSMessageHandlerInfo* next;
};

static uint32_t s_events_enabled = PSE_NONE;
static struct PSEvent* s_event_head;
static struct PSEvent* s_event_tail;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER;
static struct PSMessageHandlerInfo* s_handler_head;
static struct PSMessageHandlerInfo* s_handler_tail;

static struct PSMessageHandlerInfo* FindMessageHandler(
    const char* message_name) {
  struct PSMessageHandlerInfo* info = s_handler_head;
  while (info) {
    if (strcmp(info->message_name, message_name) == 0) {
      return info;
    }

    info = info->next;
  }

  return NULL;
}

static void EnqueueEvent(struct PSEvent* event) {
  pthread_mutex_lock(&s_lock);

  if (!s_event_tail) {
    s_event_head = s_event_tail = event;
    event->next = NULL;
  } else {
    s_event_tail->next = event;
    s_event_tail = event;
  }

  pthread_cond_signal(&s_cond);
  pthread_mutex_unlock(&s_lock);
}

static struct PSEvent* DequeueEvent(int block) {
  pthread_mutex_lock(&s_lock);

  if (block) {
    while (s_event_head == NULL) {
      pthread_cond_wait(&s_cond, &s_lock);
    }
  }

  if (s_event_head == NULL) {
    pthread_mutex_unlock(&s_lock);
    return NULL;
  }

  struct PSEvent* item = s_event_head;

  if (s_event_head == s_event_tail) {
    s_event_head = s_event_tail = NULL;
  } else {
    s_event_head = s_event_head->next;
  }

  pthread_mutex_unlock(&s_lock);

  return item;
}

struct PSEvent* PSEventTryAcquire() {
  struct PSEvent* event;
  while (1) {
    event = DequeueEvent(NO_BLOCK);
    if (NULL == event)
      break;
    if (s_events_enabled & event->type)
      break;
    /* Release filtered events & continue to acquire. */
    PSEventRelease(event);
  }
  return event;
}

struct PSEvent* PSEventWaitAcquire() {
  struct PSEvent* event;
  while (1) {
    event = DequeueEvent(BLOCK);
    if (s_events_enabled & event->type)
      break;
    /* Release filtered events & continue to acquire. */
    PSEventRelease(event);
  }
  return event;
}

void PSEventRelease(struct PSEvent* event) {
  if (event) {
    switch (event->type) {
      case PSE_INSTANCE_HANDLEMESSAGE:
        PSInterfaceVar()->Release(event->as_var);
        break;
      case PSE_INSTANCE_HANDLEINPUT:
      case PSE_INSTANCE_DIDCHANGEVIEW:
        if (event->as_resource) {
          PSInterfaceCore()->ReleaseResource(event->as_resource);
        }
        break;
      default:
        break;
    }
    free(event);
  }
}

void PSEventSetFilter(PSEventTypeMask filter) {
  s_events_enabled = filter;
  if (filter == 0) {
    static int s_warn_once = 1;
    if (s_warn_once) {
      PSInstanceWarn(
          "PSInstance::SetEnabledEvents(mask) where mask == 0 will block\n");
      PSInstanceWarn(
          "all events. This can come from PSEventSetFilter(PSE_NONE);\n");
      s_warn_once = 0;
    }
  }
}

void PSEventPost(PSEventType type) {
  assert(PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST == type ||
         PSE_MOUSELOCK_MOUSELOCKLOST == type);

  struct PSEvent* event = malloc(sizeof(struct PSEvent));
  memset(event, 0, sizeof(*event));
  event->type = type;
  EnqueueEvent(event);
}

void PSEventPostBool(PSEventType type, PP_Bool bool_value) {
  assert(PSE_INSTANCE_DIDCHANGEFOCUS == type);

  struct PSEvent* event = malloc(sizeof(struct PSEvent));
  memset(event, 0, sizeof(*event));
  event->type = type;
  event->as_bool = bool_value;
  EnqueueEvent(event);
}

void PSEventPostVar(PSEventType type, struct PP_Var var) {
  assert(PSE_INSTANCE_HANDLEMESSAGE == type);

  /* If the message is a dictionary then see if it matches one of the specific
   * handlers, then call that handler rather than queuing an event. */
  if (var.type == PP_VARTYPE_DICTIONARY) {
    struct PP_Var keys_var = PSInterfaceVarDictionary()->GetKeys(var);
    if (PSInterfaceVarArray()->GetLength(keys_var) == 1) {
      struct PP_Var key_var = PSInterfaceVarArray()->Get(keys_var, 0);
      uint32_t key_len;
      const char* key_str = PSInterfaceVar()->VarToUtf8(key_var, &key_len);
      char* key_cstr = alloca(key_len + 1);
      memcpy(key_cstr, key_str, key_len);
      key_cstr[key_len] = 0;
      PSInstanceTrace("calling handler for: %s\n", key_cstr);

      struct PSMessageHandlerInfo* handler_info = FindMessageHandler(key_cstr);
      if (handler_info) {
        struct PP_Var value_var = PSInterfaceVarDictionary()->Get(var, key_var);
        handler_info->func(key_var, value_var, handler_info->user_data);
        PSInterfaceVar()->Release(value_var);
        PSInterfaceVar()->Release(key_var);
        PSInterfaceVar()->Release(keys_var);
        return;
      }

      PSInterfaceVar()->Release(key_var);
    }

    PSInterfaceVar()->Release(keys_var);
  }

  PSInterfaceVar()->AddRef(var);
  struct PSEvent *env =  malloc(sizeof(struct PSEvent));
  memset(env, 0, sizeof(*env));
  env->type = type;
  env->as_var = var;
  EnqueueEvent(env);
}

void PSEventPostResource(PSEventType type, PP_Resource resource) {
  assert(PSE_INSTANCE_HANDLEINPUT == type ||
         PSE_INSTANCE_DIDCHANGEVIEW == type);

  if (resource) {
    PSInterfaceCore()->AddRefResource(resource);
  }
  struct PSEvent* event = malloc(sizeof(struct PSEvent));
  memset(event, 0, sizeof(*event));
  event->type = type;
  event->as_resource = resource;
  EnqueueEvent(event);
}

void PSEventRegisterMessageHandler(const char* message_name,
                                   PSMessageHandler_t func,
                                   void* user_data) {
  PSInstanceTrace("registering msg handler: %s\n", message_name);
  struct PSMessageHandlerInfo* handler = FindMessageHandler(message_name);

  if (func == NULL) {
    /* Unregister handler, if it exists */
    if (handler) {
      if (handler->prev) {
        handler->prev->next = handler->next;
      } else {
        s_handler_head = handler->next;
      }

      if (handler->next) {
        handler->next->prev = handler->prev;
      } else {
        s_handler_tail = handler->prev;
      }

      free(handler->message_name);
      free(handler);
    }
    return;
  }

  if (handler) {
    /* Already registered, change its function */
    handler->func = func;
    handler->user_data = user_data;
  } else {
    /* Not registered, append a new handler info */
    struct PSMessageHandlerInfo* handler_info =
        malloc(sizeof(struct PSMessageHandlerInfo));
    handler_info->message_name = strdup(message_name);
    handler_info->func = func;
    handler_info->user_data = user_data;
    handler_info->next = NULL;
    handler_info->prev = s_handler_tail;

    if (s_handler_tail) {
      s_handler_tail->next = handler_info;
      s_handler_tail = handler_info;
    } else {
      s_handler_head = s_handler_tail = handler_info;
    }
  }
}
