// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_

#include "media/capture/capture_export.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

struct CAPTURE_EXPORT ReadyFrameInBuffer {
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

  int buffer_id;
  int frame_feedback_id;
  std::unique_ptr<VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
      buffer_read_permission;
  mojom::VideoFrameInfoPtr frame_info;
};

// Callback interface for VideoCaptureDeviceClient to communicate with its
// clients. On some platforms, VideoCaptureDeviceClient calls these methods from
// OS or capture driver provided threads which do not have a task runner and
// cannot be posted back to. The mostly equivalent interface
// video_capture::mojom::VideoFrameHandler cannot be used by
// VideoCaptureDeviceClient directly, because creating a
// video_catpure::mojom::ScopedAccessPermission for passing into
// OnFrameReadyInBuffer() requires a thread with a task runner.
class CAPTURE_EXPORT VideoFrameReceiver {
 public:
  virtual ~VideoFrameReceiver() {}

  // Tells the VideoFrameReceiver that the producer is going to subsequently use
  // the provided buffer as one of possibly many for frame delivery via
  // OnFrameReadyInBuffer(). Note, that a call to this method does not mean that
  // the caller allows the receiver to read from or write to the buffer just
  // yet. Temporary permission to read will be given with subsequent calls to
  // OnFrameReadyInBuffer().
  virtual void OnNewBuffer(
      int32_t buffer_id,
      media::mojom::VideoBufferHandlePtr buffer_handle) = 0;

  // Tells the VideoFrameReceiver that a new frame is ready for consumption
  // in the buffer with id |buffer_id| and allows it to read the data from
  // the buffer. The producer guarantees that the buffer and its contents stay
  // alive and unchanged until VideoFrameReceiver releases the given
  // |buffer_read_permission|.
  virtual void OnFrameReadyInBuffer(
      ReadyFrameInBuffer frame,
      std::vector<ReadyFrameInBuffer> scaled_frames) = 0;

  // Tells the VideoFrameReceiver that the producer is no longer going to use
  // the buffer with id |buffer_id| for frame delivery. This may be called even
  // while the receiver is still holding |buffer_read_permission| from a call to
  // OnFrameReadInBuffer() for the same buffer. In that case, it means that the
  // caller is asking the VideoFrameReceiver to release the read permission and
  // buffer handle at its earliest convenience. After this call, a producer may
  // immediately reuse the retired |buffer_id| with a new buffer via a call to
  // OnNewBuffer().
  virtual void OnBufferRetired(int buffer_id) = 0;

  virtual void OnError(VideoCaptureError error) = 0;
  virtual void OnFrameDropped(VideoCaptureFrameDropReason reason) = 0;
  virtual void OnLog(const std::string& message) = 0;
  virtual void OnStarted() = 0;
  virtual void OnStartedUsingGpuDecode() = 0;
  virtual void OnStopped() = 0;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_FRAME_RECEIVER_H_
