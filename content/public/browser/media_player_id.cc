// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_player_id.h"

namespace content {

MediaPlayerId::MediaPlayerId(GlobalRenderFrameHostId frame_routing_id,
                             int delegate_id)
    : frame_routing_id(frame_routing_id), delegate_id(delegate_id) {}

MediaPlayerId MediaPlayerId::CreateMediaPlayerIdForTests() {
  return MediaPlayerId(GlobalRenderFrameHostId(), 0);
}

bool MediaPlayerId::operator==(const MediaPlayerId& other) const {
  return frame_routing_id == other.frame_routing_id &&
         delegate_id == other.delegate_id;
}

bool MediaPlayerId::operator!=(const MediaPlayerId& other) const {
  return frame_routing_id != other.frame_routing_id ||
         delegate_id != other.delegate_id;
}

bool MediaPlayerId::operator<(const MediaPlayerId& other) const {
  if (frame_routing_id == other.frame_routing_id)
    return delegate_id < other.delegate_id;
  return frame_routing_id < other.frame_routing_id;
}

}  // namespace content
