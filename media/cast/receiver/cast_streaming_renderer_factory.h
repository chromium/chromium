// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_FACTORY_H_
#define MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_FACTORY_H_

#include <memory>

#include "media/base/renderer_factory.h"

namespace media {
namespace cast {

// This class defines a RendererFactory used to create a CastStreamingRenderer,
// for use with Cast streaming.
//
// For a description of the cast streaming scenario, see
// cast_streaming_renderer.h in the same directory as this file.
class CastStreamingRendererFactory : public RendererFactory {
 public:
  // |renderer_factory| is the RendererFactory to be used as described below.
  explicit CastStreamingRendererFactory(
      std::unique_ptr<RendererFactory> renderer_factory);
  CastStreamingRendererFactory(const CastStreamingRendererFactory& other) =
      delete;
  CastStreamingRendererFactory(CastStreamingRendererFactory&& other) = delete;

  ~CastStreamingRendererFactory() override;

  CastStreamingRendererFactory& operator=(
      const CastStreamingRendererFactory& other) = delete;
  CastStreamingRendererFactory& operator=(
      CastStreamingRendererFactory&& other) = delete;

  // RendererFactory overrides.
  //
  // Wraps |real_renderer_factory_->CreateRenderer()|'s results with a
  // MirroringRenderer instance.
  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

 private:
  std::unique_ptr<RendererFactory> real_renderer_factory_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_FACTORY_H_
