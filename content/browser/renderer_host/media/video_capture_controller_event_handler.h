// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_

#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

typedef base::UnguessableToken VideoCaptureControllerID;

// Represents a buffer that is ready for consumption. Mirrors ReadyBuffer in
// video_capture_types.mojom.
struct ReadyBuffer {
  ReadyBuffer(int buffer_id, media::mojom::VideoFrameInfoPtr frame_info);
  ReadyBuffer(ReadyBuffer&& other);
  ~ReadyBuffer();

  ReadyBuffer& operator=(ReadyBuffer&& other);

  int buffer_id;
  media::mojom::VideoFrameInfoPtr frame_info;
};

// VideoCaptureControllerEventHandler is the interface for
// VideoCaptureController to notify clients about the events such as
// BufferReady, FrameInfo, Error, etc.

// OnError and OnEnded need to be scheduled to the end of message queue to
// guarantee some other clearing jobs are done before they are handled.
// Other methods can be forwarded synchronously.

// TODO(mcasas): https://crbug.com/654176 merge back into VideoCaptureController
class VideoCaptureControllerEventHandler {
 public:
  virtual void OnCaptureConfigurationChanged(
      const VideoCaptureControllerID& id) = 0;

  // An Error has occurred in the VideoCaptureDevice.
  virtual void OnError(const VideoCaptureControllerID& id,
                       media::VideoCaptureError error) = 0;

  virtual void OnNewBuffer(const VideoCaptureControllerID& id,
                           media::mojom::VideoBufferHandlePtr buffer_handle,
                           int buffer_id) = 0;

  // A previously created buffer has been freed and will no longer be used.
  virtual void OnBufferDestroyed(const VideoCaptureControllerID& id,
                                 int buffer_id) = 0;

  // A buffer (and optionally scaled versions of it) has been filled with a
  // captured VideoFrame.
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             const ReadyBuffer& buffer) = 0;

  // A frame was dropped - OnBufferReady() was never called for this frame. In
  // other words the frame was dropped before it reached the renderer process.
  virtual void OnFrameDropped(const VideoCaptureControllerID& id,
                              media::VideoCaptureFrameDropReason reason) = 0;

  // All subsequent buffers are guaranteed to have a sub-capture-target version
  // whose value is at least |sub_capture_target_version|.
  virtual void OnNewSubCaptureTargetVersion(
      const VideoCaptureControllerID& id,
      uint32_t sub_capture_target_version) = 0;

  virtual void OnFrameWithEmptyRegionCapture(
      const VideoCaptureControllerID& id) = 0;

  // The capture session has ended and no more frames will be sent.
  virtual void OnEnded(const VideoCaptureControllerID& id) = 0;

  // VideoCaptureDevice has successfully started the device.
  virtual void OnStarted(const VideoCaptureControllerID& id) = 0;

  virtual void OnStartedUsingGpuDecode(const VideoCaptureControllerID& id) = 0;

 protected:
  virtual ~VideoCaptureControllerEventHandler() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_EVENT_HANDLER_H_
