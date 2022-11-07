// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_FACTORY_H_
#define MEDIA_BASE_RENDERER_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_export.h"
#include "media/base/media_resource.h"
#include "media/base/overlay_info.h"
#include "media/base/renderer.h"
#include "ui/gfx/color_space.h"

namespace base {
class TaskRunner;
}

namespace media {

class AudioRendererSink;
class VideoRendererSink;

// A factory class for creating media::Renderer to be used by media pipeline.
class MEDIA_EXPORT RendererFactory {
 public:
  RendererFactory();

  RendererFactory(const RendererFactory&) = delete;
  RendererFactory& operator=(const RendererFactory&) = delete;

  virtual ~RendererFactory();

  // Creates and returns a Renderer. All methods of the created Renderer except
  // for GetMediaTime() will be called on the |media_task_runner|.
  // GetMediaTime() could be called on any thread.
  // The created Renderer can use |audio_renderer_sink| to render audio and
  // |video_renderer_sink| to render video.
  virtual std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) = 0;

  // Returns the MediaResource::Type that should be used with the renderers
  // created by this factory.
  // NOTE: Returns Type::STREAM by default.
  virtual MediaResource::Type GetRequiredMediaResourceType();
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_FACTORY_H_
