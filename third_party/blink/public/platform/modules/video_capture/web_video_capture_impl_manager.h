// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_VIDEO_CAPTURE_WEB_VIDEO_CAPTURE_IMPL_MANAGER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_VIDEO_CAPTURE_WEB_VIDEO_CAPTURE_IMPL_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class VideoCaptureImpl;
class WebString;

// TODO(hclam): This class should be renamed to VideoCaptureService.

// This class provides access to a video capture device in the browser
// process through IPC. The main function is to deliver video frames
// to a client.
//
// THREADING
//
// WebVideoCaptureImplManager lives only on the Render Main thread. All methods
// must be called on this thread.
//
// VideoFrames are delivered on the IO thread. Callbacks provided by
// a client are also called on the IO thread.
class BLINK_PLATFORM_EXPORT WebVideoCaptureImplManager {
 public:
  WebVideoCaptureImplManager();
  virtual ~WebVideoCaptureImplManager();

  // Open a device associated with the session ID.
  // This method must be called before any methods with the same ID
  // is used.
  // Returns a callback that should be used to release the acquired
  // resources.
  base::OnceClosure UseDevice(const media::VideoCaptureSessionId& id);

  // Start receiving video frames for the given session ID.
  //
  // |state_update_cb| will be called on the IO thread when capturing
  // state changes.
  // States will be one of the following four:
  // * VIDEO_CAPTURE_STATE_STARTED
  // * VIDEO_CAPTURE_STATE_STOPPED
  // * VIDEO_CAPTURE_STATE_PAUSED
  // * VIDEO_CAPTURE_STATE_ERROR
  //
  // |deliver_frame_cb| will be called on the IO thread when a video
  // frame is ready.
  //
  // Returns a callback that is used to stop capturing. Note that stopping
  // video capture is not synchronous. Client should handle the case where
  // callbacks are called after capturing is instructed to stop, typically
  // by binding the passed callbacks on a WeakPtr.
  base::OnceClosure StartCapture(
      const media::VideoCaptureSessionId& id,
      const media::VideoCaptureParams& params,
      const VideoCaptureStateUpdateCB& state_update_cb,
      const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Requests that the video capturer send a frame "soon" (e.g., to resolve
  // picture loss or quality issues).
  void RequestRefreshFrame(const media::VideoCaptureSessionId& id);

  // Requests frame delivery be suspended/resumed for a given capture session.
  void Suspend(const media::VideoCaptureSessionId& id);
  void Resume(const media::VideoCaptureSessionId& id);

  // Get supported formats supported by the device for the given session
  // ID. |callback| will be called on the IO thread.
  void GetDeviceSupportedFormats(const media::VideoCaptureSessionId& id,
                                 VideoCaptureDeviceFormatsCB callback);

  // Get supported formats currently in use for the given session ID.
  // |callback| will be called on the IO thread.
  void GetDeviceFormatsInUse(const media::VideoCaptureSessionId& id,
                             VideoCaptureDeviceFormatsCB callback);

  // Make all VideoCaptureImpl instances in the input |video_devices|
  // stop/resume delivering video frames to their clients, depends on flag
  // |suspend|. This is called in response to a RenderView-wide
  // PageHidden/Shown() event.
  // To suspend/resume an individual session, please call Suspend(id) or
  // Resume(id).
  void SuspendDevices(const MediaStreamDevices& video_devices, bool suspend);

  void OnLog(const media::VideoCaptureSessionId& id, const WebString& message);
  void OnFrameDropped(const media::VideoCaptureSessionId& id,
                      media::VideoCaptureFrameDropReason reason);

  virtual std::unique_ptr<VideoCaptureImpl> CreateVideoCaptureImplForTesting(
      const media::VideoCaptureSessionId& session_id) const;

 private:
  // Holds bookkeeping info for each VideoCaptureImpl shared by clients.
  struct DeviceEntry;

  void StopCapture(int client_id, const media::VideoCaptureSessionId& id);
  void UnrefDevice(const media::VideoCaptureSessionId& id);

  // Devices currently in use.
  WebVector<DeviceEntry> devices_;

  // This is an internal ID for identifying clients of VideoCaptureImpl.
  // The ID is global for the render process.
  int next_client_id_;

  // Hold a pointer to the Render Main message loop to check we operate on the
  // right thread.
  const scoped_refptr<base::SingleThreadTaskRunner> render_main_task_runner_;

  // Set to true if SuspendDevices(true) was called. This, along with
  // DeviceEntry::is_individually_suspended, is used to determine whether to
  // take action when suspending/resuming each device.
  bool is_suspending_all_;

  // Bound to the render thread.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebVideoCaptureImplManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebVideoCaptureImplManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_VIDEO_CAPTURE_WEB_VIDEO_CAPTURE_IMPL_MANAGER_H_
