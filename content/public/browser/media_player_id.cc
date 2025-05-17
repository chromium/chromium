// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_player_id.h"

namespace content {

MediaPlayerId::MediaPlayerId(GlobalRenderFrameHostId frame_routing_id,
                             int player_id)
    : frame_routing_id(frame_routing_id), player_id(player_id) {}

MediaPlayerId MediaPlayerId::CreateMediaPlayerIdForTests() {
  return MediaPlayerId(GlobalRenderFrameHostId(), 0);
}

}  // namespace content
