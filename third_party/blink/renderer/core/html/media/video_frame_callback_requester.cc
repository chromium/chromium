// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_frame_callback_requester.h"

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

namespace blink {

VideoFrameCallbackRequester::VideoFrameCallbackRequester(
    HTMLVideoElement& element)
    : Supplement<HTMLVideoElement>(element) {}

// static
VideoFrameCallbackRequester* VideoFrameCallbackRequester::From(
    HTMLVideoElement& element) {
  return Supplement<HTMLVideoElement>::From<VideoFrameCallbackRequester>(
      element);
}

void VideoFrameCallbackRequester::Trace(Visitor* visitor) const {
  Supplement<HTMLVideoElement>::Trace(visitor);
}

// static
const char VideoFrameCallbackRequester::kSupplementName[] =
    "VideoFrameCallbackRequester";

}  // namespace blink
