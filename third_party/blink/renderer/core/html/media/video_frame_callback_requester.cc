// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_frame_callback_requester.h"

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

namespace blink {

VideoFrameCallbackRequester::VideoFrameCallbackRequester(
    HTMLVideoElement& element)
    : element_(element) {}

void VideoFrameCallbackRequester::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
}

}  // namespace blink
