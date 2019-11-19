// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"

#include "third_party/blink/public/platform/web_media_player.h"

namespace blink {
namespace test {

MediaStubLocalFrameClient::MediaStubLocalFrameClient(
    std::unique_ptr<WebMediaPlayer> player)
    : player_(std::move(player)) {}

std::unique_ptr<WebMediaPlayer> MediaStubLocalFrameClient::CreateWebMediaPlayer(
    HTMLMediaElement&,
    const WebMediaPlayerSource&,
    WebMediaPlayerClient*) {
  DCHECK(player_) << " Empty injected player - already used?";
  return std::move(player_);
}

}  // namespace test
}  // namespace blink
