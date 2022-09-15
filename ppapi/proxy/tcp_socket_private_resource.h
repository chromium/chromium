// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketPrivateResource
    : public thunk::PPB_TCPSocket_Private_API,
      public TCPSocketResourceBase {
 public:
  // C-tor used for new sockets.
  TCPSocketPrivateResource(Connection connection, PP_Instance instance);

  // C-tor used for already accepted sockets.
  TCPSocketPrivateResource(Connection connection,
                           PP_Instance instance,
                           int pending_resource_id,
                           const PP_NetAddress_Private& local_addr,
                           const PP_NetAddress_Private& remote_addr);

  TCPSocketPrivateResource(const TCPSocketPrivateResource&) = delete;
  TCPSocketPrivateResource& operator=(const TCPSocketPrivateResource&) = delete;

  ~TCPSocketPrivateResource() override;

  // PluginResource overrides.
  PPB_TCPSocket_Private_API* AsPPB_TCPSocket_Private_API() override;

  // PPB_TCPSocket_Private_API implementation.
  int32_t Connect(const char* host,
                  uint16_t port,
                  scoped_refptr<TrackedCallback> callback) override;
  int32_t ConnectWithNetAddress(
      const PP_NetAddress_Private* addr,
      scoped_refptr<TrackedCallback> callback) override;
  PP_Bool GetLocalAddress(PP_NetAddress_Private* local_addr) override;
  PP_Bool GetRemoteAddress(PP_NetAddress_Private* remote_addr) override;
  int32_t SSLHandshake(
      const char* server_name,
      uint16_t server_port,
      scoped_refptr<TrackedCallback> callback) override;
  PP_Resource GetServerCertificate() override;
  PP_Bool AddChainBuildingCertificate(PP_Resource certificate,
                                      PP_Bool trusted) override;
  int32_t Read(char* buffer,
               int32_t bytes_to_read,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t Write(const char* buffer,
                int32_t bytes_to_write,
                scoped_refptr<TrackedCallback> callback) override;
  void Disconnect() override;
  int32_t SetOption(PP_TCPSocketOption_Private name,
                    const PP_Var& value,
                    scoped_refptr<TrackedCallback> callback) override;

  // TCPSocketResourceBase implementation.
  PP_Resource CreateAcceptedSocket(
      int pending_host_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) override;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_PRIVATE_RESOURCE_H_
