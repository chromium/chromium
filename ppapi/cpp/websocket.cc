// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/cpp/websocket.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_WebSocket_1_0>() {
  return PPB_WEBSOCKET_INTERFACE_1_0;
}

}  // namespace

WebSocket::WebSocket(const InstanceHandle& instance) {
  if (!has_interface<PPB_WebSocket_1_0>())
    return;
  PassRefFromConstructor(get_interface<PPB_WebSocket_1_0>()->Create(
    instance.pp_instance()));
}

WebSocket::~WebSocket() {
}

int32_t WebSocket::Connect(const Var& url, const Var protocols[],
    uint32_t protocol_count, const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_1_0>())
    return PP_ERROR_BADRESOURCE;

  // Convert protocols to C interface.
  PP_Var *c_protocols = NULL;
  if (protocol_count)
    c_protocols = new PP_Var[protocol_count];
  for (uint32_t i = 0; i < protocol_count; ++i)
    c_protocols[i] = protocols[i].pp_var();

  int32_t result = get_interface<PPB_WebSocket_1_0>()->Connect(
      pp_resource(), url.pp_var(), c_protocols, protocol_count,
      callback.pp_completion_callback());
  if (c_protocols)
    delete[] c_protocols;
  return result;
}

int32_t WebSocket::Close(uint16_t code, const Var& reason,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_1_0>())
    return PP_ERROR_BADRESOURCE;

  return get_interface<PPB_WebSocket_1_0>()->Close(
      pp_resource(), code, reason.pp_var(),
      callback.pp_completion_callback());
}

int32_t WebSocket::ReceiveMessage(Var* message,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_WebSocket_1_0>())
    return PP_ERROR_BADRESOURCE;

  // Initialize |message| to release old internal PP_Var of reused |message|.
  if (message)
    *message = Var();

  return get_interface<PPB_WebSocket_1_0>()->ReceiveMessage(
      pp_resource(), const_cast<PP_Var*>(&message->pp_var()),
      callback.pp_completion_callback());
}

int32_t WebSocket::SendMessage(const Var& message) {
  if (!has_interface<PPB_WebSocket_1_0>())
    return PP_ERROR_BADRESOURCE;

  return get_interface<PPB_WebSocket_1_0>()->SendMessage(
      pp_resource(), message.pp_var());
}

uint64_t WebSocket::GetBufferedAmount() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return 0;

  return get_interface<PPB_WebSocket_1_0>()->GetBufferedAmount(pp_resource());
}

uint16_t WebSocket::GetCloseCode() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return 0;

  return get_interface<PPB_WebSocket_1_0>()->GetCloseCode(pp_resource());
}

Var WebSocket::GetCloseReason() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return 0;

  return Var(PASS_REF,
      get_interface<PPB_WebSocket_1_0>()->GetCloseReason(pp_resource()));
}

bool WebSocket::GetCloseWasClean() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return false;

  PP_Bool result =
      get_interface<PPB_WebSocket_1_0>()->GetCloseWasClean(pp_resource());
  return PP_ToBool(result);
}

Var WebSocket::GetExtensions() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_1_0>()->GetExtensions(pp_resource()));
}

Var WebSocket::GetProtocol() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_1_0>()->GetProtocol(pp_resource()));
}

PP_WebSocketReadyState WebSocket::GetReadyState() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return PP_WEBSOCKETREADYSTATE_INVALID;

  return get_interface<PPB_WebSocket_1_0>()->GetReadyState(pp_resource());
}

Var WebSocket::GetURL() {
  if (!has_interface<PPB_WebSocket_1_0>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_WebSocket_1_0>()->GetURL(pp_resource()));
}

}  // namespace pp
