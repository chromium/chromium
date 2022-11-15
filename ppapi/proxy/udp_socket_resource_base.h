// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_RESOURCE_BASE_H_
#define PPAPI_PROXY_UDP_SOCKET_RESOURCE_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/udp_socket_filter.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

class ResourceMessageReplyParams;

class PPAPI_PROXY_EXPORT UDPSocketResourceBase : public PluginResource {
 protected:
  UDPSocketResourceBase(Connection connection,
                        PP_Instance instance,
                        bool private_api);

  UDPSocketResourceBase(const UDPSocketResourceBase&) = delete;
  UDPSocketResourceBase& operator=(const UDPSocketResourceBase&) = delete;

  virtual ~UDPSocketResourceBase();

  int32_t SetOptionImpl(PP_UDPSocket_Option name,
                        const PP_Var& value,
                        bool check_bind_state,
                        scoped_refptr<TrackedCallback> callback);
  int32_t BindImpl(const PP_NetAddress_Private* addr,
                   scoped_refptr<TrackedCallback> callback);
  PP_Bool GetBoundAddressImpl(PP_NetAddress_Private* addr);
  // |addr| could be NULL to indicate that an output value is not needed.
  int32_t RecvFromImpl(char* buffer,
                       int32_t num_bytes,
                       PP_Resource* addr,
                       scoped_refptr<TrackedCallback> callback);
  PP_Bool GetRecvFromAddressImpl(PP_NetAddress_Private* addr);
  int32_t SendToImpl(const char* buffer,
                     int32_t num_bytes,
                     const PP_NetAddress_Private* addr,
                     scoped_refptr<TrackedCallback> callback);
  void CloseImpl();
  int32_t JoinGroupImpl(const PP_NetAddress_Private *group,
                        scoped_refptr<TrackedCallback> callback);
  int32_t LeaveGroupImpl(const PP_NetAddress_Private *group,
                         scoped_refptr<TrackedCallback> callback);

 private:
  // IPC message handlers.
  void OnPluginMsgGeneralReply(scoped_refptr<TrackedCallback> callback,
                               const ResourceMessageReplyParams& params);
  void OnPluginMsgBindReply(const ResourceMessageReplyParams& params,
                            const PP_NetAddress_Private& bound_addr);
  void OnPluginMsgSendToReply(const ResourceMessageReplyParams& params,
                              int32_t bytes_written);

  static void SlotBecameAvailable(PP_Resource resource);
  static void SlotBecameAvailableWithLock(PP_Resource resource);

  bool private_api_;

  // |bind_called_| is true after Bind() is called, while |bound_| is true
  // after Bind() succeeds. Bind() is an asynchronous method, so the timing
  // on which of these is set is slightly different.
  bool bind_called_;
  bool bound_;
  bool closed_;

  scoped_refptr<TrackedCallback> bind_callback_;
  scoped_refptr<UDPSocketFilter> recv_filter_;

  PP_NetAddress_Private bound_addr_;

  base::queue<scoped_refptr<TrackedCallback>> sendto_callbacks_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_RESOURCE_BASE_H_
