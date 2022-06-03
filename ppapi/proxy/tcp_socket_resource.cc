// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_resource.h"

#include "base/check_op.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_net_address_api.h"

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterResourceNoLock<thunk::PPB_NetAddress_API>
    EnterNetAddressNoLock;

}  // namespace

TCPSocketResource::TCPSocketResource(Connection connection,
                                     PP_Instance instance,
                                     TCPSocketVersion version)
    : TCPSocketResourceBase(connection, instance, version) {
  DCHECK_NE(version, TCP_SOCKET_VERSION_PRIVATE);
  SendCreate(BROWSER, PpapiHostMsg_TCPSocket_Create(version));
}

TCPSocketResource::TCPSocketResource(Connection connection,
                                     PP_Instance instance,
                                     int pending_host_id,
                                     const PP_NetAddress_Private& local_addr,
                                     const PP_NetAddress_Private& remote_addr)
    : TCPSocketResourceBase(connection, instance,
                            TCP_SOCKET_VERSION_1_1_OR_ABOVE, local_addr,
                            remote_addr) {
  AttachToPendingHost(BROWSER, pending_host_id);
}

TCPSocketResource::~TCPSocketResource() {
}

thunk::PPB_TCPSocket_API* TCPSocketResource::AsPPB_TCPSocket_API() {
  return this;
}

int32_t TCPSocketResource::Bind(PP_Resource addr,
                                scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return BindImpl(&enter.object()->GetNetAddressPrivate(), callback);
}

int32_t TCPSocketResource::Connect(PP_Resource addr,
                                   scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return ConnectWithNetAddressImpl(&enter.object()->GetNetAddressPrivate(),
                                   callback);
}

PP_Resource TCPSocketResource::GetLocalAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetLocalAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

PP_Resource TCPSocketResource::GetRemoteAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetRemoteAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

int32_t TCPSocketResource::Read(char* buffer,
                                int32_t bytes_to_read,
                                scoped_refptr<TrackedCallback> callback) {
  return ReadImpl(buffer, bytes_to_read, callback);
}

int32_t TCPSocketResource::Write(const char* buffer,
                                 int32_t bytes_to_write,
                                 scoped_refptr<TrackedCallback> callback) {
  return WriteImpl(buffer, bytes_to_write, callback);
}

int32_t TCPSocketResource::Listen(int32_t backlog,
                                  scoped_refptr<TrackedCallback> callback) {
  return ListenImpl(backlog, callback);
}

int32_t TCPSocketResource::Accept(PP_Resource* accepted_tcp_socket,
                                  scoped_refptr<TrackedCallback> callback) {
  return AcceptImpl(accepted_tcp_socket, callback);
}

void TCPSocketResource::Close() {
  CloseImpl();
}

int32_t TCPSocketResource::SetOption1_1(
    PP_TCPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  return SetOptionImpl(name, value,
                       true,  // Check connect() state.
                       callback);
}

int32_t TCPSocketResource::SetOption(PP_TCPSocket_Option name,
                                     const PP_Var& value,
                                     scoped_refptr<TrackedCallback> callback) {
  return SetOptionImpl(name, value,
                       false,  // Do not check connect() state.
                       callback);
}

PP_Resource TCPSocketResource::CreateAcceptedSocket(
    int pending_host_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  return (new TCPSocketResource(connection(), pp_instance(), pending_host_id,
                                local_addr, remote_addr))->GetReference();
}

}  // namespace proxy
}  // namespace ppapi
