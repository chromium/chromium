// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_

#include "base/synchronization/waitable_event.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_client.h"

namespace gpu {
class GpuChannelHost;
}

namespace content {

// RenderMediaClient is purely plumbing to make content embedder customizations
// visible to the lower media layer.
class RenderMediaClient : public media::MediaClient {
 public:
  RenderMediaClient(const RenderMediaClient&) = delete;
  RenderMediaClient& operator=(const RenderMediaClient&) = delete;

  // Initialize RenderMediaClient and SetMediaClient(). Note that the instance
  // is not exposed because no content code needs to directly access it.
  static void Initialize();

  // MediaClient implementation.
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) final;
  bool IsSupportedAudioType(const media::AudioType& type) final;
  bool IsSupportedVideoType(const media::VideoType& type) final;
  bool IsSupportedBitstreamAudioCodec(media::AudioCodec codec) final;
  absl::optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      media::AudioParameters audio_parameters) final;

 private:
  RenderMediaClient();
  ~RenderMediaClient() override;

  void OnEstablishedGpuChannel(scoped_refptr<gpu::GpuChannelHost> host);

  // Used to indicate if optional video profile support information has been
  // retrieved from the GPU channel. May be waited upon by any thread but the
  // RenderThread since it's always signaled from the RenderThread.
  base::WaitableEvent did_update_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
