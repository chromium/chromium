// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_VPN_PROVIDER_MESSAGE_FILTER_CHROMEOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_VPN_PROVIDER_MESSAGE_FILTER_CHROMEOS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/shared_impl/vpn_provider_util.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;

// The host for PPB_VpnProvider.
// Important: The PPB_VpnProvider API is available only on Chrome OS.
class PepperVpnProviderMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperVpnProviderMessageFilter(BrowserPpapiHostImpl* host,
                                 PP_Instance instance);

  PepperVpnProviderMessageFilter(const PepperVpnProviderMessageFilter&) =
      delete;
  PepperVpnProviderMessageFilter& operator=(
      const PepperVpnProviderMessageFilter&) = delete;

  // ppapi::host::ResourceMessageFilter overrides.
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // PepperVpnProviderResourceHostProxyImpl entry points.
  void SendOnPacketReceived(const std::vector<char>& packet);
  void SendOnUnbind();

 private:
  using SuccessCallback = base::OnceClosure;
  using FailureCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;
  ~PepperVpnProviderMessageFilter() override;

  // Message handlers
  int32_t OnBind(ppapi::host::HostMessageContext* context,
                 const std::string& configuration_id,
                 const std::string& configuration_name);
  int32_t OnSendPacket(ppapi::host::HostMessageContext* context,
                       uint32_t packet_size,
                       uint32_t id);
  int32_t OnPacketReceivedReply(ppapi::host::HostMessageContext* context,
                                uint32_t id);
  // OnBind helpers
  int32_t DoBind(SuccessCallback success_callback,
                 FailureCallback failure_callback);
  void OnBindSuccess(const ppapi::host::ReplyMessageContext& context);
  void OnBindFailure(const ppapi::host::ReplyMessageContext& context,
                     const std::string& error_name,
                     const std::string& error_message);
  void OnBindReply(const ppapi::host::ReplyMessageContext& context,
                   int32_t reply);

  // OnSendPacket helpers
  int32_t DoSendPacket(const std::vector<char>& packet,
                       SuccessCallback success_callback,
                       FailureCallback failure_callback);
  void OnSendPacketSuccess(const ppapi::host::ReplyMessageContext& context,
                           uint32_t id);
  void OnSendPacketFailure(const ppapi::host::ReplyMessageContext& context,
                           uint32_t id,
                           const std::string& error_name,
                           const std::string& error_message);
  void OnSendPacketReply(const ppapi::host::ReplyMessageContext& context,
                         uint32_t id);

  // OnPacketReceived helper
  void DoPacketReceived(const std::vector<char>& packet, uint32_t id);

  GURL document_url_;
  std::string configuration_id_;
  std::string configuration_name_;

  raw_ptr<BrowserContext> browser_context_;
  std::unique_ptr<VpnServiceProxy> vpn_service_proxy_;

  bool bound_;
  std::unique_ptr<ppapi::VpnProviderSharedBuffer> send_packet_buffer_;
  std::unique_ptr<ppapi::VpnProviderSharedBuffer> recv_packet_buffer_;

  base::queue<std::vector<char>> received_packets_;

  base::WeakPtrFactory<PepperVpnProviderMessageFilter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_VPN_PROVIDER_MESSAGE_FILTER_CHROMEOS_H_
