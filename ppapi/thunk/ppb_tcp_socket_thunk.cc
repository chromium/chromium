// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_tcp_socket.idl modified Sun Sep 15 16:14:21 2013.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_tcp_socket_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create_1_0(PP_Instance instance) {
  VLOG(4) << "PPB_TCPSocket::Create_1_0()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTCPSocket1_0(instance);
}

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_TCPSocket::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTCPSocket(instance);
}

PP_Bool IsTCPSocket(PP_Resource resource) {
  VLOG(4) << "PPB_TCPSocket::IsTCPSocket()";
  EnterResource<PPB_TCPSocket_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Bind(PP_Resource tcp_socket,
             PP_Resource addr,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Bind()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Bind(addr, enter.callback()));
}

int32_t Connect(PP_Resource tcp_socket,
                PP_Resource addr,
                struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Connect()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Connect(addr, enter.callback()));
}

PP_Resource GetLocalAddress(PP_Resource tcp_socket) {
  VLOG(4) << "PPB_TCPSocket::GetLocalAddress()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetLocalAddress();
}

PP_Resource GetRemoteAddress(PP_Resource tcp_socket) {
  VLOG(4) << "PPB_TCPSocket::GetRemoteAddress()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetRemoteAddress();
}

int32_t Read(PP_Resource tcp_socket,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Read()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Read(buffer,
                                              bytes_to_read,
                                              enter.callback()));
}

int32_t Write(PP_Resource tcp_socket,
              const char* buffer,
              int32_t bytes_to_write,
              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Write()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Write(buffer,
                                               bytes_to_write,
                                               enter.callback()));
}

int32_t Listen(PP_Resource tcp_socket,
               int32_t backlog,
               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Listen()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Listen(backlog, enter.callback()));
}

int32_t Accept(PP_Resource tcp_socket,
               PP_Resource* accepted_tcp_socket,
               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::Accept()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Accept(accepted_tcp_socket,
                                                enter.callback()));
}

void Close(PP_Resource tcp_socket) {
  VLOG(4) << "PPB_TCPSocket::Close()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

int32_t SetOption1_1(PP_Resource tcp_socket,
                     PP_TCPSocket_Option name,
                     struct PP_Var value,
                     struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::SetOption1_1()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SetOption1_1(name,
                                                      value,
                                                      enter.callback()));
}

int32_t SetOption(PP_Resource tcp_socket,
                  PP_TCPSocket_Option name,
                  struct PP_Var value,
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TCPSocket::SetOption()";
  EnterResource<PPB_TCPSocket_API> enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SetOption(name,
                                                   value,
                                                   enter.callback()));
}

const PPB_TCPSocket_1_0 g_ppb_tcpsocket_thunk_1_0 = {
  &Create_1_0,
  &IsTCPSocket,
  &Connect,
  &GetLocalAddress,
  &GetRemoteAddress,
  &Read,
  &Write,
  &Close,
  &SetOption1_1
};

const PPB_TCPSocket_1_1 g_ppb_tcpsocket_thunk_1_1 = {
  &Create,
  &IsTCPSocket,
  &Bind,
  &Connect,
  &GetLocalAddress,
  &GetRemoteAddress,
  &Read,
  &Write,
  &Listen,
  &Accept,
  &Close,
  &SetOption1_1
};

const PPB_TCPSocket_1_2 g_ppb_tcpsocket_thunk_1_2 = {
  &Create,
  &IsTCPSocket,
  &Bind,
  &Connect,
  &GetLocalAddress,
  &GetRemoteAddress,
  &Read,
  &Write,
  &Listen,
  &Accept,
  &Close,
  &SetOption
};

}  // namespace

const PPB_TCPSocket_1_0* GetPPB_TCPSocket_1_0_Thunk() {
  return &g_ppb_tcpsocket_thunk_1_0;
}

const PPB_TCPSocket_1_1* GetPPB_TCPSocket_1_1_Thunk() {
  return &g_ppb_tcpsocket_thunk_1_1;
}

const PPB_TCPSocket_1_2* GetPPB_TCPSocket_1_2_Thunk() {
  return &g_ppb_tcpsocket_thunk_1_2;
}

}  // namespace thunk
}  // namespace ppapi
