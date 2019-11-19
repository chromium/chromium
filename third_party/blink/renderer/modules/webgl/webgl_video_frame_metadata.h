// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_METADATA_H_

#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class WebGLVideoFrameMetadata final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLVideoFrameMetadata* Create(
      WebMediaPlayer::VideoFrameUploadMetadata*);
  explicit WebGLVideoFrameMetadata(WebMediaPlayer::VideoFrameUploadMetadata*);

  double presentationTime() const { return presentation_time_; }
  double expectedPresentationTime() const {
    return expected_presentation_time_;
  }
  unsigned width() const { return width_; }
  unsigned height() const { return height_; }

  double presentationTimestamp() const { return presentation_timestamp_; }
  double elapsedProcessingTime() const { return elapsed_processing_time_; }
  double captureTime() const { return capture_time_; }
  unsigned presentedFrames() const { return presented_frames_; }
  unsigned rtpTimestamp() const { return rtp_timestamp_; }

 private:
  double presentation_time_ = 0;
  double expected_presentation_time_ = 0;
  unsigned width_ = 0;
  unsigned height_ = 0;

  double presentation_timestamp_ = 0;
  double elapsed_processing_time_ = 0;
  double capture_time_ = 0;
  unsigned presented_frames_ = 0;
  unsigned rtp_timestamp_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_METADATA_H_
