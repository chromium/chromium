// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIA_DISPLAY_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIA_DISPLAY_TYPE_H_

namespace blink {

// TODO(https://crbug.com/1198341): Move back into WebMediaPlayer after
// onion-souping media/blink.
enum class DisplayType {
  // Playback is happening inline.
  kInline,
  // Playback is happening either with the video fullscreen. It may also be
  // set when Blink detects that the video is effectively fullscreen even if
  // the element is not.
  kFullscreen,
  // Playback is happening in a Picture-in-Picture window.
  kPictureInPicture,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIA_DISPLAY_TYPE_H_
