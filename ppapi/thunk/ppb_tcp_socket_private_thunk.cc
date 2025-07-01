// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_TCPSocket_Private_API> EnterTCP;

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTCPSocketPrivate(instance);
}

PP_Bool IsTCPSocket(PP_Resource resource) {
  EnterTCP enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Connect(PP_Resource tcp_socket,
                const char* host,
                uint16_t port,
                PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Connect(host, port, enter.callback()));
}

int32_t ConnectWithNetAddress(PP_Resource tcp_socket,
                              const PP_NetAddress_Private* addr,
                              PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->ConnectWithNetAddress(addr, enter.callback()));
}

PP_Bool GetLocalAddress(PP_Resource tcp_socket,
                        PP_NetAddress_Private* local_addr) {
  EnterTCP enter(tcp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetLocalAddress(local_addr);
}

PP_Bool GetRemoteAddress(PP_Resource tcp_socket,
                         PP_NetAddress_Private* remote_addr) {
  EnterTCP enter(tcp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRemoteAddress(remote_addr);
}

int32_t SSLHandshake(PP_Resource tcp_socket,
                     const char* server_name,
                     uint16_t server_port,
                     PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SSLHandshake(server_name, server_port,
                                                      enter.callback()));
}

PP_Resource GetServerCertificate(PP_Resource tcp_socket) {
  EnterTCP enter(tcp_socket, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetServerCertificate();
}

PP_Bool AddChainBuildingCertificate(PP_Resource tcp_socket,
                                    PP_Resource certificate,
                                    PP_Bool trusted) {
  EnterTCP enter(tcp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->AddChainBuildingCertificate(certificate, trusted);
}

int32_t Read(PP_Resource tcp_socket,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Read(buffer, bytes_to_read,
                                              enter.callback()));
}

int32_t Write(PP_Resource tcp_socket,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Write(buffer, bytes_to_write,
                                               enter.callback()));
}

void Disconnect(PP_Resource tcp_socket) {
  EnterTCP enter(tcp_socket, true);
  if (enter.succeeded())
    enter.object()->Disconnect();
}

int32_t SetOption(PP_Resource tcp_socket,
                  PP_TCPSocketOption_Private name,
                  PP_Var value,
                  PP_CompletionCallback callback) {
  EnterTCP enter(tcp_socket, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->SetOption(name, value, enter.callback()));
}

const PPB_TCPSocket_Private_0_3 g_ppb_tcp_socket_thunk_0_3 = {
  &Create,
  &IsTCPSocket,
  &Connect,
  &ConnectWithNetAddress,
  &GetLocalAddress,
  &GetRemoteAddress,
  &SSLHandshake,
  &Read,
  &Write,
  &Disconnect
};

const PPB_TCPSocket_Private_0_4 g_ppb_tcp_socket_thunk_0_4 = {
  &Create,
  &IsTCPSocket,
  &Connect,
  &ConnectWithNetAddress,
  &GetLocalAddress,
  &GetRemoteAddress,
  &SSLHandshake,
  &GetServerCertificate,
  &AddChainBuildingCertificate,
  &Read,
  &Write,
  &Disconnect
};

const PPB_TCPSocket_Private_0_5 g_ppb_tcp_socket_thunk_0_5 = {
  &Create,
  &IsTCPSocket,
  &Connect,
  &ConnectWithNetAddress,
  &GetLocalAddress,
  &GetRemoteAddress,
  &SSLHandshake,
  &GetServerCertificate,
  &AddChainBuildingCertificate,
  &Read,
  &Write,
  &Disconnect,
  &SetOption
};

}  // namespace

const PPB_TCPSocket_Private_0_3* GetPPB_TCPSocket_Private_0_3_Thunk() {
  return &g_ppb_tcp_socket_thunk_0_3;
}

const PPB_TCPSocket_Private_0_4* GetPPB_TCPSocket_Private_0_4_Thunk() {
  return &g_ppb_tcp_socket_thunk_0_4;
}

const PPB_TCPSocket_Private_0_5* GetPPB_TCPSocket_Private_0_5_Thunk() {
  return &g_ppb_tcp_socket_thunk_0_5;
}

}  // namespace thunk
}  // namespace ppapi
