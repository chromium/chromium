// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_player_id.h"

#include "content/public/browser/render_frame_host.h"

namespace content {

MediaPlayerId::MediaPlayerId(RenderFrameHost* render_frame_host,
                             int delegate_id)
    : render_frame_host(render_frame_host), delegate_id(delegate_id) {}

MediaPlayerId MediaPlayerId::CreateMediaPlayerIdForTests() {
  return MediaPlayerId(nullptr, 0);
}

bool MediaPlayerId::operator==(const MediaPlayerId& other) const {
  return render_frame_host == other.render_frame_host &&
         delegate_id == other.delegate_id;
}

bool MediaPlayerId::operator!=(const MediaPlayerId& other) const {
  return render_frame_host != other.render_frame_host ||
         delegate_id != other.delegate_id;
}

bool MediaPlayerId::operator<(const MediaPlayerId& other) const {
  if (render_frame_host == other.render_frame_host)
    return delegate_id < other.delegate_id;
  return render_frame_host < other.render_frame_host;
}

}  // namespace content
