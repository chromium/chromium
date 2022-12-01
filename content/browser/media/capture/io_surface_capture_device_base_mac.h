// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_IO_SURFACE_CAPTURE_DEVICE_BASE_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_IO_SURFACE_CAPTURE_DEVICE_BASE_MAC_H_

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_device.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/io_surface.h"

namespace content {

// Common base class for the shared functionality of the two classes that
// capture frames as IOSurfaces, DesktopCaptureDeviceMac and
// ScreenCaptureKitDeviceMac.
class CONTENT_EXPORT IOSurfaceCaptureDeviceBase
    : public media::VideoCaptureDevice {
 public:
  IOSurfaceCaptureDeviceBase();
  ~IOSurfaceCaptureDeviceBase() override;

  // OnStart is called by AllocateAndStart.
  virtual void OnStart() = 0;

  // OnStop is called by StopAndDeAllocate.
  virtual void OnStop() = 0;

  // media::VideoCaptureDevice overrides.
  void RequestRefreshFrame() override;

 protected:
  void OnReceivedIOSurfaceFromStream(
      gfx::ScopedInUseIOSurface io_surface,
      const media::VideoCaptureFormat& capture_format,
      const gfx::Rect& visible_rect);
  void SendLastReceivedIOSurfaceToClient();

  // Given a source frame size `source_size`, and `capture_params_`, compute the
  // appropriate frame size and store it in `frame_size`. If custom letterboxing
  // is to be performed, store the destination rectangle for the source content
  // in `dest_rect_in_frame`.
  void ComputeFrameSizeAndDestRect(const gfx::Size& source_size,
                                   gfx::Size& frame_size,
                                   gfx::RectF& dest_rect_in_frame) const;

  Client* client() { return client_.get(); }
  const media::VideoCaptureParams& capture_params() { return capture_params_; }

 private:
  // media::VideoCaptureDevice:
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void StopAndDeAllocate() final;

  // This class assumes single threaded access.
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<Client> client_;

  // The parameters that were specified to AllocateAndStart.
  media::VideoCaptureParams capture_params_;

  // The time of the first call to SendLastReceivedIOSurfaceToClient. Used to
  // compute the timestamp of subsequent frames.
  base::TimeTicks first_frame_time_;

  // The most recent arguments to OnReceivedIOSurfaceFromStream. If no other
  // frames come in, then this will be repeatedly sent at `min_frame_rate_`.
  gfx::ScopedInUseIOSurface last_received_io_surface_;
  media::VideoCaptureFormat last_received_capture_format_;
  gfx::Rect last_visible_rect_;

  base::WeakPtrFactory<IOSurfaceCaptureDeviceBase> weak_factory_base_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_IO_SURFACE_CAPTURE_DEVICE_BASE_MAC_H_
