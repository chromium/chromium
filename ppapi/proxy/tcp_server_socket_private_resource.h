// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SERVER_SOCKET_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_TCP_SERVER_SOCKET_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_tcp_server_socket_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TCPServerSocketPrivateResource
    : public PluginResource,
      public thunk::PPB_TCPServerSocket_Private_API {
 public:
  TCPServerSocketPrivateResource(Connection connection, PP_Instance instance);

  TCPServerSocketPrivateResource(const TCPServerSocketPrivateResource&) =
      delete;
  TCPServerSocketPrivateResource& operator=(
      const TCPServerSocketPrivateResource&) = delete;

  ~TCPServerSocketPrivateResource() override;

  // PluginResource implementation.
  thunk::PPB_TCPServerSocket_Private_API* AsPPB_TCPServerSocket_Private_API()
      override;

  // PPB_TCPServerSocket_Private_API implementation.
  int32_t Listen(const PP_NetAddress_Private* addr,
                 int32_t backlog,
                 scoped_refptr<TrackedCallback> callback) override;
  int32_t Accept(PP_Resource* tcp_socket,
                 scoped_refptr<TrackedCallback> callback) override;
  int32_t GetLocalAddress(PP_NetAddress_Private* addr) override;
  void StopListening() override;

 private:
  enum State {
    STATE_BEFORE_LISTENING,
    STATE_LISTENING,
    STATE_CLOSED
  };

  // IPC message handlers.
  void OnPluginMsgListenReply(const ResourceMessageReplyParams& params,
                              const PP_NetAddress_Private& local_addr);
  void OnPluginMsgAcceptReply(PP_Resource* tcp_socket,
                              const ResourceMessageReplyParams& params,
                              int pending_resource_id,
                              const PP_NetAddress_Private& local_addr,
                              const PP_NetAddress_Private& remote_addr);

  State state_;
  PP_NetAddress_Private local_addr_;

  scoped_refptr<TrackedCallback> listen_callback_;
  scoped_refptr<TrackedCallback> accept_callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SERVER_SOCKET_PRIVATE_RESOURCE_H_
