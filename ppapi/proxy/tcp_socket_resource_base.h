// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_
#define PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {

class PPB_X509Certificate_Fields;
class PPB_X509Certificate_Private_Shared;

namespace proxy {

class PPAPI_PROXY_EXPORT TCPSocketResourceBase : public PluginResource {
 protected:
  // C-tor used for new sockets.
  TCPSocketResourceBase(Connection connection,
                        PP_Instance instance,
                        TCPSocketVersion version);

  // C-tor used for already accepted sockets.
  TCPSocketResourceBase(Connection connection,
                        PP_Instance instance,
                        TCPSocketVersion version,
                        const PP_NetAddress_Private& local_addr,
                        const PP_NetAddress_Private& remote_addr);

  TCPSocketResourceBase(const TCPSocketResourceBase&) = delete;
  TCPSocketResourceBase& operator=(const TCPSocketResourceBase&) = delete;

  virtual ~TCPSocketResourceBase();

  // Implemented by subclasses to create resources for accepted sockets.
  virtual PP_Resource CreateAcceptedSocket(
      int pending_host_id,
      const PP_NetAddress_Private& local_addr,
      const PP_NetAddress_Private& remote_addr) = 0;

  int32_t BindImpl(const PP_NetAddress_Private* addr,
                   scoped_refptr<TrackedCallback> callback);
  int32_t ConnectImpl(const char* host,
                      uint16_t port,
                      scoped_refptr<TrackedCallback> callback);
  int32_t ConnectWithNetAddressImpl(const PP_NetAddress_Private* addr,
                                    scoped_refptr<TrackedCallback> callback);
  PP_Bool GetLocalAddressImpl(PP_NetAddress_Private* local_addr);
  PP_Bool GetRemoteAddressImpl(PP_NetAddress_Private* remote_addr);
  int32_t SSLHandshakeImpl(const char* server_name,
                           uint16_t server_port,
                           scoped_refptr<TrackedCallback> callback);
  PP_Resource GetServerCertificateImpl();
  PP_Bool AddChainBuildingCertificateImpl(PP_Resource certificate,
                                          PP_Bool trusted);
  int32_t ReadImpl(char* buffer,
                   int32_t bytes_to_read,
                   scoped_refptr<TrackedCallback> callback);
  int32_t WriteImpl(const char* buffer,
                    int32_t bytes_to_write,
                    scoped_refptr<TrackedCallback> callback);
  int32_t ListenImpl(int32_t backlog, scoped_refptr<TrackedCallback> callback);
  int32_t AcceptImpl(PP_Resource* accepted_tcp_socket,
                     scoped_refptr<TrackedCallback> callback);
  void CloseImpl();
  int32_t SetOptionImpl(PP_TCPSocket_Option name,
                        const PP_Var& value,
                        bool check_connect_state,
                        scoped_refptr<TrackedCallback> callback);

  void PostAbortIfNecessary(scoped_refptr<TrackedCallback>* callback);

  // IPC message handlers.
  void OnPluginMsgBindReply(const ResourceMessageReplyParams& params,
                            const PP_NetAddress_Private& local_addr);
  void OnPluginMsgConnectReply(const ResourceMessageReplyParams& params,
                               const PP_NetAddress_Private& local_addr,
                               const PP_NetAddress_Private& remote_addr);
  void OnPluginMsgSSLHandshakeReply(
      const ResourceMessageReplyParams& params,
      const PPB_X509Certificate_Fields& certificate_fields);
  void OnPluginMsgReadReply(const ResourceMessageReplyParams& params,
                            const std::string& data);
  void OnPluginMsgWriteReply(const ResourceMessageReplyParams& params);
  void OnPluginMsgListenReply(const ResourceMessageReplyParams& params);
  void OnPluginMsgAcceptReply(const ResourceMessageReplyParams& params,
                              int pending_host_id,
                              const PP_NetAddress_Private& local_addr,
                              const PP_NetAddress_Private& remote_addr);
  void OnPluginMsgSetOptionReply(const ResourceMessageReplyParams& params);

  scoped_refptr<TrackedCallback> bind_callback_;
  scoped_refptr<TrackedCallback> connect_callback_;
  scoped_refptr<TrackedCallback> ssl_handshake_callback_;
  scoped_refptr<TrackedCallback> read_callback_;
  scoped_refptr<TrackedCallback> write_callback_;
  scoped_refptr<TrackedCallback> listen_callback_;
  scoped_refptr<TrackedCallback> accept_callback_;
  base::queue<scoped_refptr<TrackedCallback>> set_option_callbacks_;

  TCPSocketState state_;
  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_NetAddress_Private local_addr_;
  PP_NetAddress_Private remote_addr_;

  scoped_refptr<PPB_X509Certificate_Private_Shared> server_certificate_;

  std::vector<std::vector<char> > trusted_certificates_;
  std::vector<std::vector<char> > untrusted_certificates_;

  PP_Resource* accepted_tcp_socket_;

 private:
  void RunCallback(scoped_refptr<TrackedCallback> callback, int32_t pp_result);

  TCPSocketVersion version_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TCP_SOCKET_RESOURCE_BASE_H_
