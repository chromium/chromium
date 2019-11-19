// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_

#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

struct CONTENT_EXPORT MediaPlayerId {
  static MediaPlayerId CreateMediaPlayerIdForTests();
  MediaPlayerId() = delete;

  MediaPlayerId(RenderFrameHost* render_frame_host, int delegate_id);
  bool operator==(const MediaPlayerId&) const;
  bool operator!=(const MediaPlayerId&) const;
  bool operator<(const MediaPlayerId&) const;

  RenderFrameHost* render_frame_host = nullptr;
  int delegate_id = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_
