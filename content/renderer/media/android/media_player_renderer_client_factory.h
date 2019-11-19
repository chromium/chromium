// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "media/base/android/stream_texture_wrapper.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace media {
class MojoRendererFactory;
}

namespace content {

// The default class for creating a MediaPlayerRendererClient
// and its associated MediaPlayerRenderer.
class CONTENT_EXPORT MediaPlayerRendererClientFactory
    : public media::RendererFactory {
 public:
  using GetStreamTextureWrapperCB =
      base::RepeatingCallback<media::ScopedStreamTextureWrapper()>;

  MediaPlayerRendererClientFactory(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory,
      const GetStreamTextureWrapperCB& get_stream_texture_wrapper_cb);
  ~MediaPlayerRendererClientFactory() override;

  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      const media::RequestOverlayInfoCB& request_surface_cb,
      const gfx::ColorSpace& target_color_space) override;

  // The MediaPlayerRenderer uses a Type::URL.
  media::MediaResource::Type GetRequiredMediaResourceType() override;

 private:
  GetStreamTextureWrapperCB get_stream_texture_wrapper_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_FACTORY_H_
