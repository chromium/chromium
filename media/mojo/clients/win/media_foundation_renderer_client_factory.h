// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace media {

// The default class for creating a MediaFoundationRendererClient
// and its associated MediaFoundationRenderer.
class MediaFoundationRendererClientFactory : public media::RendererFactory {
 public:
  MediaFoundationRendererClientFactory(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory);
  ~MediaFoundationRendererClientFactory() override;

  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_surface_cb,
      const gfx::ColorSpace& target_color_space) override;

  // The MediaFoundationRenderer uses a Type::URL.
  media::MediaResource::Type GetRequiredMediaResourceType() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_WIN_MEDIA_FOUNDATION_RENDERER_CLIENT_FACTORY_H_
