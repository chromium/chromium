// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

VideoFrameHandle::VideoFrameHandle(scoped_refptr<media::VideoFrame> frame,
                                   ExecutionContext* context)
    : frame_(std::move(frame)) {
  DCHECK(frame_);
  DCHECK(context);

  close_auditor_ = VideoFrameLogger::From(*context).GetCloseAuditor();

  DCHECK(close_auditor_);
}

VideoFrameHandle::VideoFrameHandle(
    scoped_refptr<media::VideoFrame> frame,
    scoped_refptr<VideoFrameLogger::VideoFrameCloseAuditor> close_auditor)
    : frame_(std::move(frame)), close_auditor_(std::move(close_auditor)) {
  DCHECK(frame_);
  DCHECK(close_auditor_);
}

VideoFrameHandle::~VideoFrameHandle() {
  // If we still have a valid |close_auditor_|, Invalidate() was never
  // called and corresponding frames never received a call to close() before
  // being garbage collected.
  if (close_auditor_)
    close_auditor_->ReportUnclosedFrame();
}

scoped_refptr<media::VideoFrame> VideoFrameHandle::frame() {
  WTF::MutexLocker locker(mutex_);
  return frame_;
}

void VideoFrameHandle::Invalidate() {
  WTF::MutexLocker locker(mutex_);
  frame_.reset();
  close_auditor_.reset();
}

}  // namespace blink
