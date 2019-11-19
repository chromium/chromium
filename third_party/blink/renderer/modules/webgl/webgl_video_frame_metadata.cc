// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_video_frame_metadata.h"

namespace blink {

WebGLVideoFrameMetadata* WebGLVideoFrameMetadata::Create(
    WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr) {
  return MakeGarbageCollected<WebGLVideoFrameMetadata>(frame_metadata_ptr);
}

WebGLVideoFrameMetadata::WebGLVideoFrameMetadata(
    WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr) {
  presentation_time_ = frame_metadata_ptr->timestamp.InMicrosecondsF();
  expected_presentation_time_ =
      frame_metadata_ptr->expected_timestamp.InMicrosecondsF();
  width_ = frame_metadata_ptr->visible_rect.width();
  height_ = frame_metadata_ptr->visible_rect.height();
}

}  // namespace blink
