// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_TYPES_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_TYPES_H_

#include <memory>

#include "media/capture/capture_export.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

struct CAPTURE_EXPORT ReadyFrameInBuffer {
  ReadyFrameInBuffer();
  ReadyFrameInBuffer(
      int buffer_id,
      int frame_feedback_id,
      std::unique_ptr<
          VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          buffer_read_permission,
      mojom::VideoFrameInfoPtr frame_info);
  ReadyFrameInBuffer(ReadyFrameInBuffer&& other);
  ~ReadyFrameInBuffer();

  ReadyFrameInBuffer& operator=(ReadyFrameInBuffer&& other);

  int buffer_id = 0;
  int frame_feedback_id = 0;
  std::unique_ptr<VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
      buffer_read_permission = nullptr;
  mojom::VideoFrameInfoPtr frame_info = nullptr;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_TYPES_H_
