// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_EVENT_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_EVENT_HANDLER_H_

#include <vector>

#include "content/renderer/media/batching_media_log.h"

namespace content {

// RenderMediaEventHandler is an implementation of
// BatchingMediaLog::EventHandler that forwards events to the browser process.
class CONTENT_EXPORT RenderMediaEventHandler
    : public BatchingMediaLog::EventHandler {
 public:
  RenderMediaEventHandler() = default;
  ~RenderMediaEventHandler() override = default;
  void SendQueuedMediaEvents(std::vector<media::MediaLogEvent>) override;
  void OnWebMediaPlayerDestroyed() override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_EVENT_HANDLER_H_
