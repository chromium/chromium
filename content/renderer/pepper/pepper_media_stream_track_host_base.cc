// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_math.h"
#include "content/common/pepper_file_util.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

using ppapi::host::HostMessageContext;
using ppapi::proxy::SerializedHandle;

namespace content {

PepperMediaStreamTrackHostBase::PepperMediaStreamTrackHostBase(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      buffer_manager_(this) {}

PepperMediaStreamTrackHostBase::~PepperMediaStreamTrackHostBase() {}

bool PepperMediaStreamTrackHostBase::InitBuffers(int32_t number_of_buffers,
                                                 int32_t buffer_size,
                                                 TrackType track_type) {
  DCHECK_GT(number_of_buffers, 0);
  DCHECK_GT(buffer_size,
            static_cast<int32_t>(sizeof(ppapi::MediaStreamBuffer::Header)));
  // Make each buffer 4 byte aligned.
  base::CheckedNumeric<int32_t> buffer_size_aligned = buffer_size;
  // TODO(amistry): "buffer size" might not == "buffer stride", in the same way
  // that width != stride in an image buffer.
  buffer_size_aligned += (4 - buffer_size % 4);

  // TODO(penghuang): |HostAllocateSharedMemoryBuffer| uses sync IPC. We should
  // avoid it.
  base::CheckedNumeric<uint32_t> size = number_of_buffers * buffer_size_aligned;
  if (!size.IsValid())
    return false;

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size.ValueOrDie());
  if (!region.IsValid())
    return false;

  SerializedHandle handle(
      host_->ShareUnsafeSharedMemoryRegionWithRemote(region));
  if (!buffer_manager_.SetBuffers(number_of_buffers,
                                  buffer_size_aligned.ValueOrDie(),
                                  std::move(region), true)) {
    return false;
  }

  bool readonly = (track_type == kRead);
  std::vector<SerializedHandle> handles;
  handles.push_back(std::move(handle));
  host()->SendUnsolicitedReplyWithHandles(
      pp_resource(),
      PpapiPluginMsg_MediaStreamTrack_InitBuffers(
          number_of_buffers, buffer_size_aligned.ValueOrDie(), readonly),
      &handles);
  return true;
}

void PepperMediaStreamTrackHostBase::SendEnqueueBufferMessageToPlugin(
    int32_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, buffer_manager_.number_of_buffers());
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_MediaStreamTrack_EnqueueBuffer(index));
}

void PepperMediaStreamTrackHostBase::SendEnqueueBuffersMessageToPlugin(
    const std::vector<int32_t>& indices) {
  DCHECK_GE(indices.size(), 0U);
  host()->SendUnsolicitedReply(pp_resource(),
      PpapiPluginMsg_MediaStreamTrack_EnqueueBuffers(indices));
}

int32_t PepperMediaStreamTrackHostBase::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperMediaStreamTrackHostBase, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_MediaStreamTrack_EnqueueBuffer, OnHostMsgEnqueueBuffer)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_MediaStreamTrack_Close,
                                        OnHostMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return ppapi::host::ResourceHost::OnResourceMessageReceived(msg, context);
}

int32_t PepperMediaStreamTrackHostBase::OnHostMsgEnqueueBuffer(
    HostMessageContext* context,
    int32_t index) {
  buffer_manager_.EnqueueBuffer(index);
  return PP_OK;
}

int32_t PepperMediaStreamTrackHostBase::OnHostMsgClose(
    HostMessageContext* context) {
  OnClose();
  return PP_OK;
}

}  // namespace content
