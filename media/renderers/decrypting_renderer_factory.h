// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_DECRYPTING_RENDERER_FACTORY_H_
#define MEDIA_RENDERERS_DECRYPTING_RENDERER_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"

namespace media {

class MediaLog;

// Simple RendererFactory wrapper class. It wraps any Renderer created by the
// underlying factory, and returns it as a DecryptingRenderer.
//
// See DecryptingRenderer for more information.
//
// The caller must guarantee that the returned DecryptingRenderer will never
// be initialized with a |media_resource| of type MediaResource::Type::URL.
class MEDIA_EXPORT DecryptingRendererFactory final : public RendererFactory {
 public:
  DecryptingRendererFactory(
      MediaLog* media_log,
      std::unique_ptr<media::RendererFactory> renderer_factory);

  DecryptingRendererFactory(const DecryptingRendererFactory&) = delete;
  DecryptingRendererFactory& operator=(const DecryptingRendererFactory&) =
      delete;

  ~DecryptingRendererFactory() final;

  // RendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;

 private:
  raw_ptr<MediaLog> media_log_;

  std::unique_ptr<media::RendererFactory> renderer_factory_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_DECRYPTING_RENDERER_FACTORY_H_
