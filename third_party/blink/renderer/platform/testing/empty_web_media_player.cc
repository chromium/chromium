// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"

#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/web_time_range.h"

namespace blink {

WebMediaPlayer::LoadTiming EmptyWebMediaPlayer::Load(
    LoadType,
    const WebMediaPlayerSource&,
    CorsMode,
    bool is_cache_disabled) {
  return LoadTiming::kImmediate;
}

WebTimeRanges EmptyWebMediaPlayer::Buffered() const {
  return WebTimeRanges();
}

WebTimeRanges EmptyWebMediaPlayer::Seekable() const {
  return WebTimeRanges();
}

gfx::Size EmptyWebMediaPlayer::NaturalSize() const {
  return gfx::Size();
}

gfx::Size EmptyWebMediaPlayer::VisibleSize() const {
  return gfx::Size();
}

WebString EmptyWebMediaPlayer::GetErrorMessage() const {
  return WebString();
}

scoped_refptr<media::VideoFrame>
EmptyWebMediaPlayer::GetCurrentFrameThenUpdate() {
  return nullptr;
}

std::optional<media::VideoFrame::ID> EmptyWebMediaPlayer::CurrentFrameId()
    const {
  return std::nullopt;
}

}  // namespace blink
