// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_frame_converter.h"

namespace media {

VideoFrameConverter::VideoFrameConverter() = default;

VideoFrameConverter::~VideoFrameConverter() = default;

void VideoFrameConverter::Destroy() {
  delete this;
}

void VideoFrameConverter::Initialize(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    OutputCB output_cb) {
  parent_task_runner_ = std::move(parent_task_runner);
  output_cb_ = std::move(output_cb);
}

void VideoFrameConverter::ConvertFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(output_cb_);

  output_cb_.Run(std::move(frame));
}

void VideoFrameConverter::AbortPendingFrames() {}

bool VideoFrameConverter::HasPendingFrames() const {
  return false;
}

}  // namespace media

namespace std {

void default_delete<media::VideoFrameConverter>::operator()(
    media::VideoFrameConverter* ptr) const {
  ptr->Destroy();
}

}  // namespace std
