// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_vpn_provider_message_filter_chromeos.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/pepper_vpn_provider_resource_host_proxy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace {

// Shared memory buffer configuration.
const size_t kMaxBufferedPackets = 128;
const size_t kMaxPacketSize = 2048;                               // 2 KB
const size_t kBufferSize = kMaxBufferedPackets * kMaxPacketSize;  // 256 KB

class PepperVpnProviderResourceHostProxyImpl
    : public content::PepperVpnProviderResourceHostProxy {
 public:
  explicit PepperVpnProviderResourceHostProxyImpl(
      base::WeakPtr<content::PepperVpnProviderMessageFilter>
          vpn_message_filter);

  void SendOnPacketReceived(const std::vector<char>& packet) override;
  void SendOnUnbind() override;

 private:
  base::WeakPtr<content::PepperVpnProviderMessageFilter> vpn_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(PepperVpnProviderResourceHostProxyImpl);
};

PepperVpnProviderResourceHostProxyImpl::PepperVpnProviderResourceHostProxyImpl(
    base::WeakPtr<content::PepperVpnProviderMessageFilter> vpn_message_filter)
    : vpn_message_filter_(vpn_message_filter) {}

void PepperVpnProviderResourceHostProxyImpl::SendOnPacketReceived(
    const std::vector<char>& packet) {
  if (vpn_message_filter_)
    vpn_message_filter_->SendOnPacketReceived(packet);
}

void PepperVpnProviderResourceHostProxyImpl::SendOnUnbind() {
  if (vpn_message_filter_)
    vpn_message_filter_->SendOnUnbind();
}

}  // namespace

namespace content {

PepperVpnProviderMessageFilter::PepperVpnProviderMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance)
    : browser_context_(nullptr), bound_(false) {
  DCHECK(host);

  document_url_ = host->GetDocumentURLForInstance(instance);

  int render_process_id, unused;
  bool result =
      host->GetRenderFrameIDsForInstance(instance, &render_process_id, &unused);
  DCHECK(result);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);
  DCHECK(render_process_host);

  browser_context_ = render_process_host->GetBrowserContext();

  vpn_service_proxy_ =
      content::GetContentClient()->browser()->GetVpnServiceProxy(
          browser_context_);
}

PepperVpnProviderMessageFilter::~PepperVpnProviderMessageFilter() {
  if (bound_ && resource_host()) {
    resource_host()->host()->SendUnsolicitedReply(
        resource_host()->pp_resource(), PpapiPluginMsg_VpnProvider_OnUnbind());
  }
}

scoped_refptr<base::TaskRunner>
PepperVpnProviderMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_VpnProvider_Bind::ID:
    case PpapiHostMsg_VpnProvider_SendPacket::ID:
    case PpapiHostMsg_VpnProvider_OnPacketReceivedReply::ID:
      return base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  }
  return nullptr;
}

int32_t PepperVpnProviderMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVpnProviderMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VpnProvider_Bind, OnBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VpnProvider_SendPacket,
                                      OnSendPacket)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VpnProvider_OnPacketReceivedReply, OnPacketReceivedReply)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperVpnProviderMessageFilter::SendOnPacketReceived(
    const std::vector<char>& packet) {
  if (packet.size() == 0 || packet.size() > kMaxPacketSize)
    return;

  uint32_t id;
  if (recv_packet_buffer_ && recv_packet_buffer_->GetAvailable(&id)) {
    recv_packet_buffer_->SetAvailable(id, false);
    DoPacketReceived(packet, id);
  } else {
    received_packets_.push(packet);
  }
}

void PepperVpnProviderMessageFilter::SendOnUnbind() {
  configuration_name_.clear();
  configuration_id_.clear();
  bound_ = false;

  send_packet_buffer_.reset();
  recv_packet_buffer_.reset();

  if (resource_host()) {
    resource_host()->host()->SendUnsolicitedReply(
        resource_host()->pp_resource(), PpapiPluginMsg_VpnProvider_OnUnbind());
  }
}

int32_t PepperVpnProviderMessageFilter::OnBind(
    ppapi::host::HostMessageContext* context,
    const std::string& configuration_id,
    const std::string& configuration_name) {
  if (!vpn_service_proxy_)
    return PP_ERROR_FAILED;
  if (!content::GetContentClient()->browser()->IsPepperVpnProviderAPIAllowed(
          browser_context_, document_url_)) {
    LOG(ERROR) << "Host " << document_url_.host()
               << " cannot use vpnProvider API";
    return PP_ERROR_NOACCESS;
  }

  configuration_id_ = configuration_id;
  configuration_name_ = configuration_name;

  return DoBind(base::Bind(&PepperVpnProviderMessageFilter::OnBindSuccess,
                           weak_factory_.GetWeakPtr(),
                           context->MakeReplyMessageContext()),
                base::Bind(&PepperVpnProviderMessageFilter::OnBindFailure,
                           weak_factory_.GetWeakPtr(),
                           context->MakeReplyMessageContext()));
}

int32_t PepperVpnProviderMessageFilter::OnSendPacket(
    ppapi::host::HostMessageContext* context,
    uint32_t packet_size,
    uint32_t id) {
  if (!vpn_service_proxy_)
    return PP_ERROR_FAILED;
  if (packet_size > kMaxPacketSize)
    return PP_ERROR_MESSAGE_TOO_BIG;

  char* packet_pointer = static_cast<char*>(send_packet_buffer_->GetBuffer(id));
  std::vector<char> packet(packet_pointer, packet_pointer + packet_size);

  return DoSendPacket(
      packet, base::Bind(&PepperVpnProviderMessageFilter::OnSendPacketSuccess,
                         weak_factory_.GetWeakPtr(),
                         context->MakeReplyMessageContext(), id),
      base::Bind(&PepperVpnProviderMessageFilter::OnSendPacketFailure,
                 weak_factory_.GetWeakPtr(), context->MakeReplyMessageContext(),
                 id));
}

int32_t PepperVpnProviderMessageFilter::OnPacketReceivedReply(
    ppapi::host::HostMessageContext* context,
    uint32_t id) {
  if (!received_packets_.empty()) {
    DoPacketReceived(received_packets_.front(), id);
    received_packets_.pop();
  } else {
    recv_packet_buffer_->SetAvailable(id, true);
  }

  return PP_OK;
}

int32_t PepperVpnProviderMessageFilter::DoBind(
    SuccessCallback success_callback,
    FailureCallback failure_callback) {
  // Initialize shared memory
  if (!send_packet_buffer_ || !recv_packet_buffer_) {
    base::UnsafeSharedMemoryRegion send_buffer =
        base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    base::UnsafeSharedMemoryRegion recv_buffer =
        base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    if (!send_buffer.IsValid() || !recv_buffer.IsValid())
      return PP_ERROR_NOMEMORY;

    base::WritableSharedMemoryMapping send_mapping = send_buffer.Map();
    base::WritableSharedMemoryMapping recv_mapping = recv_buffer.Map();
    if (!send_mapping.IsValid() || !recv_mapping.IsValid())
      return PP_ERROR_NOMEMORY;

    send_packet_buffer_ = std::make_unique<ppapi::VpnProviderSharedBuffer>(
        kMaxBufferedPackets, kMaxPacketSize, std::move(send_buffer),
        std::move(send_mapping));
    recv_packet_buffer_ = std::make_unique<ppapi::VpnProviderSharedBuffer>(
        kMaxBufferedPackets, kMaxPacketSize, std::move(recv_buffer),
        std::move(recv_mapping));
  }

  vpn_service_proxy_->Bind(
      document_url_.host(), configuration_id_, configuration_name_,
      success_callback, failure_callback,
      std::make_unique<PepperVpnProviderResourceHostProxyImpl>(
          weak_factory_.GetWeakPtr()));

  return PP_OK_COMPLETIONPENDING;
}

void PepperVpnProviderMessageFilter::OnBindSuccess(
    const ppapi::host::ReplyMessageContext& context) {
  bound_ = true;

  context.params.AppendHandle(
      ppapi::proxy::SerializedHandle(send_packet_buffer_->DuplicateRegion()));
  context.params.AppendHandle(
      ppapi::proxy::SerializedHandle(recv_packet_buffer_->DuplicateRegion()));

  OnBindReply(context, PP_OK);
}

void PepperVpnProviderMessageFilter::OnBindFailure(
    const ppapi::host::ReplyMessageContext& context,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(ERROR) << "PepperVpnProviderMessageFilter::OnBindFailure(): "
             << "error_name: "
             << "\"" << error_name << "\", "
             << "error_message: "
             << "\"" << error_message << "\"";

  // Clear buffers on error.
  send_packet_buffer_.reset();
  recv_packet_buffer_.reset();

  OnBindReply(context, PP_ERROR_FAILED);
}

void PepperVpnProviderMessageFilter::OnBindReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t reply) {
  SendReply(context, PpapiPluginMsg_VpnProvider_BindReply(
                         kMaxBufferedPackets, kMaxPacketSize, reply));
}

int32_t PepperVpnProviderMessageFilter::DoSendPacket(
    const std::vector<char>& packet,
    SuccessCallback success_callback,
    FailureCallback failure_callback) {
  vpn_service_proxy_->SendPacket(document_url_.host(), packet, success_callback,
                                 failure_callback);
  return PP_OK_COMPLETIONPENDING;
}

void PepperVpnProviderMessageFilter::OnSendPacketSuccess(
    const ppapi::host::ReplyMessageContext& context,
    uint32_t id) {
  OnSendPacketReply(context, id);
}

void PepperVpnProviderMessageFilter::OnSendPacketFailure(
    const ppapi::host::ReplyMessageContext& context,
    uint32_t id,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(ERROR) << "PepperVpnProviderMessageFilter::OnSendPacketFailure(): "
             << "error_name: "
             << "\"" << error_name << "\", "
             << "error_message: "
             << "\"" << error_message << "\"";
  OnSendPacketReply(context, id);
}

void PepperVpnProviderMessageFilter::OnSendPacketReply(
    const ppapi::host::ReplyMessageContext& context,
    uint32_t id) {
  SendReply(context, PpapiPluginMsg_VpnProvider_SendPacketReply(id));
}

void PepperVpnProviderMessageFilter::DoPacketReceived(
    const std::vector<char>& packet,
    uint32_t id) {
  uint32_t packet_size = base::checked_cast<uint32_t>(packet.size());
  DCHECK_GT(packet_size, 0U);

  const void* packet_pointer = &packet.front();
  memcpy(recv_packet_buffer_->GetBuffer(id), packet_pointer, packet_size);

  if (resource_host()) {
    resource_host()->host()->SendUnsolicitedReply(
        resource_host()->pp_resource(),
        PpapiPluginMsg_VpnProvider_OnPacketReceived(packet_size, id));
  }
}

}  // namespace content
