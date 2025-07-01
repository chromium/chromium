// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VPN_PROVIDER_RESOURCE_H_
#define PPAPI_PROXY_VPN_PROVIDER_RESOURCE_H_

#include <memory>

#include "base/containers/queue.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/vpn_provider_util.h"
#include "ppapi/thunk/ppb_vpn_provider_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT VpnProviderResource
    : public PluginResource,
      public thunk::PPB_VpnProvider_API {
 public:
  VpnProviderResource(Connection connection, PP_Instance instance);

  VpnProviderResource(const VpnProviderResource&) = delete;
  VpnProviderResource& operator=(const VpnProviderResource&) = delete;

  virtual ~VpnProviderResource();

  // PluginResource implementation.
  virtual thunk::PPB_VpnProvider_API* AsPPB_VpnProvider_API() override;

  // PPB_VpnProvider_API implementation.
  virtual int32_t Bind(const PP_Var& configuration_id,
                       const PP_Var& configuration_name,
                       const scoped_refptr<TrackedCallback>& callback) override;
  virtual int32_t SendPacket(
      const PP_Var& packet,
      const scoped_refptr<TrackedCallback>& callback) override;
  virtual int32_t ReceivePacket(
      PP_Var* packet,
      const scoped_refptr<TrackedCallback>& callback) override;

 private:
  // PluginResource overrides.
  virtual void OnReplyReceived(const ResourceMessageReplyParams& params,
                               const IPC::Message& msg) override;

  // PPB_VpnProvider IPC Replies
  void OnPluginMsgBindReply(const ResourceMessageReplyParams& params,
                            uint32_t queue_size,
                            uint32_t max_packet_size,
                            int32_t result);
  void OnPluginMsgSendPacketReply(const ResourceMessageReplyParams& params,
                                  uint32_t id);

  // Browser callbacks
  void OnPluginMsgOnUnbindReceived(const ResourceMessageReplyParams& params);
  void OnPluginMsgOnPacketReceived(const ResourceMessageReplyParams& params,
                                   uint32_t packet_size,
                                   uint32_t id);

  // Picks up a received packet and moves it to user buffer. This method is used
  // in both ReceivePacket() for fast returning path, and in
  // OnPluginMsgOnPacketReceived() for delayed callback invocations.
  void WritePacket();

  // Sends a packet to the browser. This method is used in both SendPacket() for
  // fast path, and in OnPluginMsgSendPacketReply for processing previously
  // queued packets.
  int32_t DoSendPacket(const PP_Var& packet, uint32_t id);

  scoped_refptr<TrackedCallback> bind_callback_;
  scoped_refptr<TrackedCallback> send_packet_callback_;
  scoped_refptr<TrackedCallback> receive_packet_callback_;

  // Keeps a pointer to the provided callback variable. Received packet will be
  // copied to this variable on ready.
  PP_Var* receive_packet_callback_var_;

  std::unique_ptr<ppapi::VpnProviderSharedBuffer> send_packet_buffer_;
  std::unique_ptr<ppapi::VpnProviderSharedBuffer> recv_packet_buffer_;

  base::queue<PP_Var> send_packets_;
  base::queue<scoped_refptr<Var>> received_packets_;

  // Connection bound state
  bool bound_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VPN_PROVIDER_RESOURCE_H_
