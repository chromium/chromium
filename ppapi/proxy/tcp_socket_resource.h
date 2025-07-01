// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/thunk/ppb_tcp_socket_api.h"

namespace ppapi {

namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketResource : public thunk::PPB_TCPSocket_API,
                                             public TCPSocketResourceBase {
 public:
  // C-tor used for new sockets created.
  TCPSocketResource(Connection connection,
                    PP_Instance instance,
                    TCPSocketVersion version);

  TCPSocketResource(const TCPSocketResource&) = delete;
  TCPSocketResource& operator=(const TCPSocketResource&) = delete;

  ~TCPSocketResource() override;

  // PluginResource overrides.
  thunk::PPB_TCPSocket_API* AsPPB_TCPSocket_API() override;

  // thunk::PPB_TCPSocket_API implementation.
  int32_t Bind(PP_Resource addr,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t Connect(PP_Resource addr,
                  scoped_refptr<TrackedCallback> callback) override;
  PP_Resource GetLocalAddress() override;
  PP_Resource GetRemoteAddress() override;
  int32_t Read(char* buffer,
               int32_t bytes_to_read,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t Write(const char* buffer,
                int32_t bytes_to_write,
                scoped_refptr<TrackedCallback> callback) override;
  int32_t Listen(int32_t backlog,
                 scoped_refptr<TrackedCallback> callback) override;
  int32_t Accept(PP_Resource* accepted_tcp_socket,
                 scoped_refptr<TrackedCallback> callback) override;
  void Close() override;
  int32_t SetOption1_1(
      PP_TCPSocket_Option name,
      const PP_Var& value,
      scoped_refptr<TrackedCallback> callback) override;
  int32_t SetOption(PP_TCPSocket_Option name,
                    const PP_Var& value,
                    scoped_refptr<TrackedCallback> callback) override;

  // TCPSocketResourceBase implementation.
  PP_Resource CreateAcceptedSocket(
      int pending_host_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) override;

 private:
  // C-tor used for accepted sockets.
  TCPSocketResource(Connection connection,
                    PP_Instance instance,
                    int pending_host_id,
                    const PP_NetAddress_Private& local_addr,
                    const PP_NetAddress_Private& remote_addr);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_H_
