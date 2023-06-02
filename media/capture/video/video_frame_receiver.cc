// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_frame_receiver.h"

#include "base/task/bind_post_task.h"

namespace media {

ReadyFrameInBuffer::ReadyFrameInBuffer() = default;

ReadyFrameInBuffer::ReadyFrameInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission,
    mojom::VideoFrameInfoPtr frame_info)
    : buffer_id(buffer_id),
      frame_feedback_id(frame_feedback_id),
      buffer_read_permission(std::move(buffer_read_permission)),
      frame_info(std::move(frame_info)) {}

ReadyFrameInBuffer::ReadyFrameInBuffer(ReadyFrameInBuffer&& other)
    : buffer_id(other.buffer_id),
      frame_feedback_id(other.frame_feedback_id),
      buffer_read_permission(std::move(other.buffer_read_permission)),
      frame_info(std::move(other.frame_info)) {}

ReadyFrameInBuffer::~ReadyFrameInBuffer() = default;

ReadyFrameInBuffer& ReadyFrameInBuffer::operator=(ReadyFrameInBuffer&& other) {
  buffer_id = other.buffer_id;
  frame_feedback_id = other.frame_feedback_id;
  buffer_read_permission = std::move(other.buffer_read_permission);
  frame_info = std::move(other.frame_info);
  return *this;
}

ScopedFrameDoneHelper::ScopedFrameDoneHelper(base::OnceClosure done_callback)
    : base::ScopedClosureRunner(
          base::BindPostTaskToCurrentDefault(std::move(done_callback))) {}

ScopedFrameDoneHelper::~ScopedFrameDoneHelper() = default;

}  // namespace media
