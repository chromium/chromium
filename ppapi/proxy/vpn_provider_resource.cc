// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/vpn_provider_resource.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {
namespace proxy {

VpnProviderResource::VpnProviderResource(Connection connection,
                                         PP_Instance instance)
    : PluginResource(connection, instance),
      bind_callback_(nullptr),
      send_packet_callback_(nullptr),
      receive_packet_callback_(nullptr),
      receive_packet_callback_var_(nullptr),
      bound_(false) {
  SendCreate(BROWSER, PpapiHostMsg_VpnProvider_Create());
}

VpnProviderResource::~VpnProviderResource() {}

thunk::PPB_VpnProvider_API* VpnProviderResource::AsPPB_VpnProvider_API() {
  return this;
}

void VpnProviderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(VpnProviderResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(PpapiPluginMsg_VpnProvider_OnUnbind,
                                        OnPluginMsgOnUnbindReceived)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VpnProvider_OnPacketReceived,
        OnPluginMsgOnPacketReceived)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

int32_t VpnProviderResource::Bind(
    const PP_Var& configuration_id,
    const PP_Var& configuration_name,
    const scoped_refptr<TrackedCallback>& callback) {
  if (TrackedCallback::IsPending(bind_callback_))
    return PP_ERROR_INPROGRESS;

  StringVar* configuration_id_var = StringVar::FromPPVar(configuration_id);
  if (!configuration_id_var)
    return PP_ERROR_BADARGUMENT;
  StringVar* configuration_name_var = StringVar::FromPPVar(configuration_name);
  if (!configuration_name_var)
    return PP_ERROR_BADARGUMENT;

  bind_callback_ = callback;

  Call<PpapiPluginMsg_VpnProvider_BindReply>(
      BROWSER, PpapiHostMsg_VpnProvider_Bind(configuration_id_var->value(),
                                             configuration_name_var->value()),
      base::Bind(&VpnProviderResource::OnPluginMsgBindReply, this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t VpnProviderResource::SendPacket(
    const PP_Var& packet,
    const scoped_refptr<TrackedCallback>& callback) {
  if (!bound_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(send_packet_callback_))
    return PP_ERROR_INPROGRESS;
  if (!ArrayBufferVar::FromPPVar(packet))
    return PP_ERROR_BADARGUMENT;

  uint32_t id;
  if (send_packet_buffer_.get() && send_packet_buffer_->GetAvailable(&id)) {
    // Send packet immediately
    send_packet_buffer_->SetAvailable(id, false);
    return DoSendPacket(packet, id);
  } else {
    // Packet will be sent later
    send_packet_callback_ = callback;
    PpapiGlobals::Get()->GetVarTracker()->AddRefVar(packet);
    send_packets_.push(packet);

    return PP_OK_COMPLETIONPENDING;
  }
}

int32_t VpnProviderResource::DoSendPacket(const PP_Var& packet, uint32_t id) {
  // Convert packet to std::vector<char>, then send it.
  scoped_refptr<ArrayBufferVar> packet_arraybuffer =
      ArrayBufferVar::FromPPVar(packet);
  if (!packet_arraybuffer.get())
    return PP_ERROR_BADARGUMENT;

  uint32_t packet_size = packet_arraybuffer->ByteLength();
  if (packet_size > send_packet_buffer_->max_packet_size())
    return PP_ERROR_MESSAGE_TOO_BIG;

  char* packet_pointer = static_cast<char*>(packet_arraybuffer->Map());
  memcpy(send_packet_buffer_->GetBuffer(id), packet_pointer, packet_size);
  packet_arraybuffer->Unmap();

  Call<PpapiPluginMsg_VpnProvider_SendPacketReply>(
      BROWSER, PpapiHostMsg_VpnProvider_SendPacket(packet_size, id),
      base::Bind(&VpnProviderResource::OnPluginMsgSendPacketReply, this));

  return PP_OK;
}

int32_t VpnProviderResource::ReceivePacket(
    PP_Var* packet,
    const scoped_refptr<TrackedCallback>& callback) {
  if (TrackedCallback::IsPending(receive_packet_callback_))
    return PP_ERROR_INPROGRESS;

  // Return previously received packet.
  if (!received_packets_.empty()) {
    receive_packet_callback_var_ = packet;
    WritePacket();
    return PP_OK;
  }

  // Or retain packet var and install callback.
  receive_packet_callback_var_ = packet;
  receive_packet_callback_ = callback;

  return PP_OK_COMPLETIONPENDING;
}

void VpnProviderResource::OnPluginMsgOnUnbindReceived(
    const ResourceMessageReplyParams& params) {
  bound_ = false;

  // Cleanup in-flight packets.
  while (!received_packets_.empty()) {
    received_packets_.pop();
  }
  while (!send_packets_.empty()) {
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(send_packets_.front());
    send_packets_.pop();
  }

  send_packet_buffer_.reset();
  recv_packet_buffer_.reset();
}

void VpnProviderResource::OnPluginMsgOnPacketReceived(
    const ResourceMessageReplyParams& params,
    uint32_t packet_size,
    uint32_t id) {
  DCHECK_LE(packet_size, recv_packet_buffer_->max_packet_size());
  if (!bound_) {
    // Ignore packet and mark shared memory as available
    Post(BROWSER, PpapiHostMsg_VpnProvider_OnPacketReceivedReply(id));
    return;
  }

  // Append received packet to queue.
  void* packet_pointer = recv_packet_buffer_->GetBuffer(id);
  scoped_refptr<Var> packet_var(
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferVar(packet_size,
                                                               packet_pointer));
  received_packets_.push(packet_var);

  // Mark shared memory as available for next packet
  Post(BROWSER, PpapiHostMsg_VpnProvider_OnPacketReceivedReply(id));

  if (!TrackedCallback::IsPending(receive_packet_callback_) ||
      TrackedCallback::IsScheduledToRun(receive_packet_callback_)) {
    return;
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(receive_packet_callback_);
  WritePacket();
  callback->Run(PP_OK);
}

void VpnProviderResource::OnPluginMsgBindReply(
    const ResourceMessageReplyParams& params,
    uint32_t queue_size,
    uint32_t max_packet_size,
    int32_t result) {
  if (!TrackedCallback::IsPending(bind_callback_))
    return;

  if (params.result() == PP_OK) {
    base::UnsafeSharedMemoryRegion send_shm;
    base::UnsafeSharedMemoryRegion recv_shm;
    params.TakeUnsafeSharedMemoryRegionAtIndex(0, &send_shm);
    params.TakeUnsafeSharedMemoryRegionAtIndex(1, &recv_shm);
    if (!send_shm.IsValid() || !recv_shm.IsValid()) {
      NOTREACHED();
      return;
    }
    base::WritableSharedMemoryMapping send_mapping = send_shm.Map();
    base::WritableSharedMemoryMapping recv_mapping = recv_shm.Map();
    if (!send_mapping.IsValid() || !recv_mapping.IsValid()) {
      NOTREACHED();
      return;
    }

    size_t buffer_size = queue_size * max_packet_size;
    if (send_shm.GetSize() < buffer_size || recv_shm.GetSize() < buffer_size) {
      NOTREACHED();
      return;
    }
    send_packet_buffer_ = std::make_unique<ppapi::VpnProviderSharedBuffer>(
        queue_size, max_packet_size, std::move(send_shm),
        std::move(send_mapping));
    recv_packet_buffer_ = std::make_unique<ppapi::VpnProviderSharedBuffer>(
        queue_size, max_packet_size, std::move(recv_shm),
        std::move(recv_mapping));

    bound_ = (result == PP_OK);
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(bind_callback_);
  callback->Run(params.result() ? params.result() : result);
}

void VpnProviderResource::OnPluginMsgSendPacketReply(
    const ResourceMessageReplyParams& params,
    uint32_t id) {
  if (!send_packets_.empty() && bound_) {
    // Process remaining packets
    DoSendPacket(send_packets_.front(), id);
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(send_packets_.front());
    send_packets_.pop();
  } else {
    send_packet_buffer_->SetAvailable(id, true);

    // Available slots - Run callback to process new packets.
    if (TrackedCallback::IsPending(send_packet_callback_)) {
      scoped_refptr<TrackedCallback> callback;
      callback.swap(send_packet_callback_);
      callback->Run(PP_OK);
    }
  }
}

void VpnProviderResource::WritePacket() {
  if (!receive_packet_callback_var_)
    return;

  *receive_packet_callback_var_ = received_packets_.front()->GetPPVar();
  received_packets_.pop();
  receive_packet_callback_var_ = nullptr;
}

}  // namespace proxy
}  // namespace ppapi
