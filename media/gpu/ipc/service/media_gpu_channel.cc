// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_gpu_channel.h"

#include "base/single_thread_task_runner.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"

namespace media {

class MediaGpuChannelDispatchHelper {
 public:
  MediaGpuChannelDispatchHelper(MediaGpuChannel* channel, int32_t routing_id)
      : channel_(channel), routing_id_(routing_id) {}

  bool Send(IPC::Message* msg) { return channel_->Send(msg); }

  void OnCreateVideoDecoder(const VideoDecodeAccelerator::Config& config,
                            int32_t decoder_route_id,
                            IPC::Message* reply_message) {
    channel_->OnCreateVideoDecoder(routing_id_, config, decoder_route_id,
                                   reply_message);
  }

 private:
  MediaGpuChannel* const channel_;
  const int32_t routing_id_;
  DISALLOW_COPY_AND_ASSIGN(MediaGpuChannelDispatchHelper);
};

MediaGpuChannel::MediaGpuChannel(
    gpu::GpuChannel* channel,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : channel_(channel), overlay_factory_cb_(overlay_factory_cb) {}

MediaGpuChannel::~MediaGpuChannel() = default;

bool MediaGpuChannel::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool MediaGpuChannel::OnMessageReceived(const IPC::Message& message) {
  MediaGpuChannelDispatchHelper helper(this, message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaGpuChannel, message)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(
        GpuCommandBufferMsg_CreateVideoDecoder, &helper,
        MediaGpuChannelDispatchHelper::OnCreateVideoDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaGpuChannel::OnCreateVideoDecoder(
    int32_t command_buffer_route_id,
    const VideoDecodeAccelerator::Config& config,
    int32_t decoder_route_id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "MediaGpuChannel::OnCreateVideoDecoder");
  gpu::CommandBufferStub* stub =
      channel_->LookupCommandBuffer(command_buffer_route_id);
  // Only allow stubs that have a ContextGroup, that is, the GLES2 ones. Later
  // code assumes the ContextGroup is valid.
  if (!stub || !stub->decoder_context()->GetContextGroup()) {
    reply_message->set_reply_error();
    Send(reply_message);
    return;
  }
  GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
      decoder_route_id, stub, stub->channel()->io_task_runner(),
      overlay_factory_cb_);
  bool succeeded = decoder->Initialize(config);
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(reply_message,
                                                           succeeded);
  Send(reply_message);

  // decoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

}  // namespace media
