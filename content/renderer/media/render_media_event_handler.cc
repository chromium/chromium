// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_event_handler.h"
#include "content/public/renderer/render_thread.h"

namespace content {

void RenderMediaEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogRecord> events_to_send) {
  for (auto& record : events_to_send)
    record.id = log_id_;
  GetMediaInternalRecordLogRemote().Log(events_to_send);
}

RenderMediaEventHandler::RenderMediaEventHandler(
    media::MediaPlayerLoggingID player_id)
    : log_id_(player_id) {
  DCHECK(RenderThread::Get())
      << "RenderMediaEventHandler must be constructed on the render thread";
}

RenderMediaEventHandler::~RenderMediaEventHandler() = default;

// This media log doesn't care, since the RenderThread outlives us for
// chrome://media-internals.
void RenderMediaEventHandler::OnWebMediaPlayerDestroyed() {}

content::mojom::MediaInternalLogRecords&
RenderMediaEventHandler::GetMediaInternalRecordLogRemote() {
  if (!media_internal_log_remote_) {
    RenderThread::Get()->BindHostReceiver(
        media_internal_log_remote_.BindNewPipeAndPassReceiver());
  }
  return *media_internal_log_remote_;
}

}  // namespace content
