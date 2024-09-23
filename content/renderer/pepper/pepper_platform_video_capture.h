// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {
class PepperMediaDeviceManager;
class PepperVideoCaptureHost;

// This object must only be used on the thread it's constructed on.
class PepperPlatformVideoCapture {
 public:
  PepperPlatformVideoCapture(int render_frame_id,
                             const std::string& device_id,
                             PepperVideoCaptureHost* handler);

  PepperPlatformVideoCapture(const PepperPlatformVideoCapture&) = delete;
  PepperPlatformVideoCapture& operator=(const PepperPlatformVideoCapture&) =
      delete;

  virtual ~PepperPlatformVideoCapture();

  // Detaches the event handler and stops sending notifications to it.
  void DetachEventHandler();

  void StartCapture(const media::VideoCaptureParams& params);
  void StopCapture();

 private:
  void OnDeviceOpened(int request_id, bool succeeded, const std::string& label);
  void OnStateUpdate(blink::VideoCaptureState state);
  void OnFrameReady(scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks estimated_capture_time);

  // Can return NULL if the RenderFrame referenced by |render_frame_id_| has
  // gone away.
  PepperMediaDeviceManager* GetMediaDeviceManager();

  const int render_frame_id_;
  const std::string device_id_;

  std::string label_;
  base::UnguessableToken session_id_;
  base::OnceClosure release_device_cb_;
  base::OnceClosure stop_capture_cb_;

  raw_ptr<PepperVideoCaptureHost> handler_;

  // Whether we have a pending request to open a device. We have to make sure
  // there isn't any pending request before this object goes away.
  bool pending_open_device_;
  int pending_open_device_id_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<PepperPlatformVideoCapture> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_
