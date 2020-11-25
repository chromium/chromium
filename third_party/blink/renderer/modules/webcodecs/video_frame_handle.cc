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

  destruction_auditor_ =
      VideoFrameLogger::From(*context).GetDestructionAuditor();

  DCHECK(destruction_auditor_);
}

VideoFrameHandle::VideoFrameHandle(
    scoped_refptr<media::VideoFrame> frame,
    scoped_refptr<VideoFrameLogger::VideoFrameDestructionAuditor> reporter)
    : frame_(std::move(frame)), destruction_auditor_(std::move(reporter)) {
  DCHECK(frame_);
  DCHECK(destruction_auditor_);
}

VideoFrameHandle::~VideoFrameHandle() {
  // If we still have a valid |destruction_auditor_|, Invalidate() was never
  // called and corresponding frames never received a call to destroy() before
  // being garbage collected.
  if (destruction_auditor_)
    destruction_auditor_->ReportUndestroyedFrame();
}

scoped_refptr<media::VideoFrame> VideoFrameHandle::frame() {
  WTF::MutexLocker locker(mutex_);
  return frame_;
}

void VideoFrameHandle::Invalidate() {
  WTF::MutexLocker locker(mutex_);
  frame_.reset();
  destruction_auditor_.reset();
}

}  // namespace blink
