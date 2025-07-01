// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_private_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"

namespace ppapi {
namespace proxy {

TCPSocketPrivateResource::TCPSocketPrivateResource(Connection connection,
                                                   PP_Instance instance)
    : TCPSocketResourceBase(connection, instance, TCP_SOCKET_VERSION_PRIVATE) {
  SendCreate(BROWSER, PpapiHostMsg_TCPSocket_CreatePrivate());
}

TCPSocketPrivateResource::TCPSocketPrivateResource(
    Connection connection,
    PP_Instance instance,
    int pending_resource_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr)
    : TCPSocketResourceBase(connection, instance, TCP_SOCKET_VERSION_PRIVATE,
                            local_addr, remote_addr) {
  AttachToPendingHost(BROWSER, pending_resource_id);
}

TCPSocketPrivateResource::~TCPSocketPrivateResource() {
}

thunk::PPB_TCPSocket_Private_API*
TCPSocketPrivateResource::AsPPB_TCPSocket_Private_API() {
  return this;
}

int32_t TCPSocketPrivateResource::Connect(
    const char* host,
    uint16_t port,
    scoped_refptr<TrackedCallback> callback) {
  return ConnectImpl(host, port, callback);
}

int32_t TCPSocketPrivateResource::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  return ConnectWithNetAddressImpl(addr, callback);
}

PP_Bool TCPSocketPrivateResource::GetLocalAddress(
    PP_NetAddress_Private* local_addr) {
  return GetLocalAddressImpl(local_addr);
}

PP_Bool TCPSocketPrivateResource::GetRemoteAddress(
    PP_NetAddress_Private* remote_addr) {
  return GetRemoteAddressImpl(remote_addr);
}

int32_t TCPSocketPrivateResource::SSLHandshake(
    const char* server_name,
    uint16_t server_port,
    scoped_refptr<TrackedCallback> callback) {
  return SSLHandshakeImpl(server_name, server_port, callback);
}

PP_Resource TCPSocketPrivateResource::GetServerCertificate() {
  return GetServerCertificateImpl();
}

PP_Bool TCPSocketPrivateResource::AddChainBuildingCertificate(
    PP_Resource certificate,
    PP_Bool trusted) {
  return AddChainBuildingCertificateImpl(certificate, trusted);
}

int32_t TCPSocketPrivateResource::Read(
    char* buffer,
    int32_t bytes_to_read,
    scoped_refptr<TrackedCallback> callback) {
  return ReadImpl(buffer, bytes_to_read, callback);
}

int32_t TCPSocketPrivateResource::Write(
    const char* buffer,
    int32_t bytes_to_write,
    scoped_refptr<TrackedCallback> callback) {
  return WriteImpl(buffer, bytes_to_write, callback);
}

void TCPSocketPrivateResource::Disconnect() {
  CloseImpl();
}

int32_t TCPSocketPrivateResource::SetOption(
    PP_TCPSocketOption_Private name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  switch (name) {
    case PP_TCPSOCKETOPTION_PRIVATE_INVALID:
      return PP_ERROR_BADARGUMENT;
    case PP_TCPSOCKETOPTION_PRIVATE_NO_DELAY:
      return SetOptionImpl(PP_TCPSOCKET_OPTION_NO_DELAY, value,
                           true,  // Check connect() state.
                           callback);
    default:
      NOTREACHED();
  }
}

PP_Resource TCPSocketPrivateResource::CreateAcceptedSocket(
    int /* pending_host_id */,
    const PP_NetAddress_Private& /* local_addr */,
    const PP_NetAddress_Private& /* remote_addr */) {
  NOTREACHED();
}

}  // namespace proxy
}  // namespace ppapi
