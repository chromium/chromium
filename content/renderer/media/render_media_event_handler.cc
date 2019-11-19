// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_event_handler.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_thread.h"

namespace content {

void RenderMediaEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogEvent> events_to_send) {
  RenderThread::Get()->Send(new ViewHostMsg_MediaLogEvents(events_to_send));
}

// This media log doesn't care, since the RenderThread outlives us for
// chrome://media-internals.
void RenderMediaEventHandler::OnWebMediaPlayerDestroyed() {}

}  // namespace content
