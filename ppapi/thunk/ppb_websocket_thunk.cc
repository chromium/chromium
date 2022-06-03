// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_websocket.idl modified Mon May  6 10:11:29 2013.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_websocket_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_WebSocket::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateWebSocket(instance);
}

PP_Bool IsWebSocket(PP_Resource resource) {
  VLOG(4) << "PPB_WebSocket::IsWebSocket()";
  EnterResource<PPB_WebSocket_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Connect(PP_Resource web_socket,
                struct PP_Var url,
                const struct PP_Var protocols[],
                uint32_t protocol_count,
                struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_WebSocket::Connect()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, callback, false);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Connect(url, protocols, protocol_count,
                                                 enter.callback()));
}

int32_t Close(PP_Resource web_socket,
              uint16_t code,
              struct PP_Var reason,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_WebSocket::Close()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, callback, false);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Close(code, reason, enter.callback()));
}

int32_t ReceiveMessage(PP_Resource web_socket,
                       struct PP_Var* message,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_WebSocket::ReceiveMessage()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, callback, false);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->ReceiveMessage(message, enter.callback()));
}

int32_t SendMessage(PP_Resource web_socket, struct PP_Var message) {
  VLOG(4) << "PPB_WebSocket::SendMessage()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return enter.retval();
  return enter.object()->SendMessage(message);
}

uint64_t GetBufferedAmount(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetBufferedAmount()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return 0;
  return enter.object()->GetBufferedAmount();
}

uint16_t GetCloseCode(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetCloseCode()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return 0;
  return enter.object()->GetCloseCode();
}

struct PP_Var GetCloseReason(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetCloseReason()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetCloseReason();
}

PP_Bool GetCloseWasClean(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetCloseWasClean()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetCloseWasClean();
}

struct PP_Var GetExtensions(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetExtensions()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetExtensions();
}

struct PP_Var GetProtocol(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetProtocol()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetProtocol();
}

PP_WebSocketReadyState GetReadyState(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetReadyState()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_WEBSOCKETREADYSTATE_INVALID;
  return enter.object()->GetReadyState();
}

struct PP_Var GetURL(PP_Resource web_socket) {
  VLOG(4) << "PPB_WebSocket::GetURL()";
  EnterResource<PPB_WebSocket_API> enter(web_socket, false);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetURL();
}

const PPB_WebSocket_1_0 g_ppb_websocket_thunk_1_0 = {&Create,
                                                     &IsWebSocket,
                                                     &Connect,
                                                     &Close,
                                                     &ReceiveMessage,
                                                     &SendMessage,
                                                     &GetBufferedAmount,
                                                     &GetCloseCode,
                                                     &GetCloseReason,
                                                     &GetCloseWasClean,
                                                     &GetExtensions,
                                                     &GetProtocol,
                                                     &GetReadyState,
                                                     &GetURL};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_WebSocket_1_0* GetPPB_WebSocket_1_0_Thunk() {
  return &g_ppb_websocket_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
