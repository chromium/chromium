// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/batching_media_log.h"
#include "third_party/blink/public/web/web_media_inspector.h"

namespace content {

// InspectorMediaEventHandler is an implementation of
// BatchingMediaLog::EventHandler that reformats events and sends them to
// the devtools inspector.
class CONTENT_EXPORT InspectorMediaEventHandler
    : public BatchingMediaLog::EventHandler {
 public:
  explicit InspectorMediaEventHandler(blink::MediaInspectorContext*);
  ~InspectorMediaEventHandler() override = default;
  void SendQueuedMediaEvents(std::vector<media::MediaLogRecord>) override;
  void OnWebMediaPlayerDestroyed() override;

 private:
  raw_ptr<blink::MediaInspectorContext, DanglingUntriaged> inspector_context_;
  blink::WebString player_id_;
  bool video_player_destroyed_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_INSPECTOR_MEDIA_EVENT_HANDLER_H_
