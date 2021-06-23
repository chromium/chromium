// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_
#define MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_

#include <memory>

#include "base/unguessable_token.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
struct CreateVideoEncoderParams;
}

namespace gpu {
class GpuChannel;
}

namespace media {

class MediaGpuChannelDispatchHelper;
class MediaGpuChannelFilter;

class MediaGpuChannel : public IPC::Listener, public IPC::Sender {
 public:
  MediaGpuChannel(gpu::GpuChannel* channel,
                  const base::UnguessableToken& channel_token,
                  const AndroidOverlayMojoFactoryCB& overlay_factory_cb);
  ~MediaGpuChannel() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

 private:
  friend class MediaGpuChannelDispatchHelper;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers.
  void OnCreateVideoDecoder(int32_t command_buffer_route_id,
                            const VideoDecodeAccelerator::Config& config,
                            int32_t route_id,
                            IPC::Message* reply_message);
  void OnCreateVideoEncoder(int32_t command_buffer_route_id,
                            const CreateVideoEncoderParams& params,
                            IPC::Message* reply_message);

  gpu::GpuChannel* const channel_;
  scoped_refptr<MediaGpuChannelFilter> filter_;
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaGpuChannel);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_MEDIA_GPU_CHANNEL_H_
