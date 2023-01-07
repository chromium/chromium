// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_messaging.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void PostMessage(PP_Instance instance, struct PP_Var message) {
  VLOG(4) << "PPB_Messaging::PostMessage()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->PostMessage(instance, message);
}

int32_t RegisterMessageHandler(PP_Instance instance,
                               void* user_data,
                               const struct PPP_MessageHandler_0_2* handler,
                               PP_Resource message_loop) {
  VLOG(4) << "PPB_Messaging::RegisterMessageHandler()";
  EnterInstance enter(instance);
  if (enter.failed())
    return enter.retval();
  return enter.functions()->RegisterMessageHandler(instance, user_data, handler,
                                                   message_loop);
}

void UnregisterMessageHandler(PP_Instance instance) {
  VLOG(4) << "PPB_Messaging::UnregisterMessageHandler()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->UnregisterMessageHandler(instance);
}

const PPB_Messaging_1_0 g_ppb_messaging_thunk_1_0 = {&PostMessage};

const PPB_Messaging_1_2 g_ppb_messaging_thunk_1_2 = {
    &PostMessage, &RegisterMessageHandler, &UnregisterMessageHandler};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Messaging_1_0* GetPPB_Messaging_1_0_Thunk() {
  return &g_ppb_messaging_thunk_1_0;
}

PPAPI_THUNK_EXPORT const PPB_Messaging_1_2* GetPPB_Messaging_1_2_Thunk() {
  return &g_ppb_messaging_thunk_1_2;
}

}  // namespace thunk
}  // namespace ppapi
