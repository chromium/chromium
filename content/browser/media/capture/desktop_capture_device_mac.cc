// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

namespace {

class DesktopCaptureDeviceMac : public media::VideoCaptureDevice {
 public:
  DesktopCaptureDeviceMac(CGDirectDisplayID display_id)
      : display_id_(display_id), weak_factory_(this) {}

  ~DesktopCaptureDeviceMac() override = default;

  static float ComputeMinFrameRate(float requested_frame_rate) {
    // Set a minimum frame rate of 5 fps, unless the requested frame rate is
    // even lower.
    constexpr float kMinFrameRate = 5.f;

    // Don't send frames at more than 80% the requested rate, because doing so
    // can stochastically toggle between repeated and new frames.
    constexpr float kRequestedFrameRateFactor = 0.8f;

    return std::min(requested_frame_rate * kRequestedFrameRateFactor,
                    kMinFrameRate);
  }

  // media::VideoCaptureDevice:
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(client && !client_);
    client_ = std::move(client);

    requested_format_ = params.requested_format;
    requested_format_.pixel_format = media::PIXEL_FORMAT_NV12;
    DCHECK_GT(requested_format_.frame_size.GetArea(), 0);
    DCHECK_GT(requested_format_.frame_rate, 0);
    min_frame_rate_ = ComputeMinFrameRate(requested_format_.frame_rate);

    CGDisplayStreamFrameAvailableHandler handler =
        ^(CGDisplayStreamFrameStatus status, uint64_t display_time,
          IOSurfaceRef frame_surface, CGDisplayStreamUpdateRef update_ref) {
          if (status == kCGDisplayStreamFrameStatusFrameComplete)
            OnReceivedIOSurfaceFromStream(frame_surface);
        };

    base::ScopedCFTypeRef<CFDictionaryRef> properties;
    {
      float max_frame_time = 1.f / requested_format_.frame_rate;
      base::ScopedCFTypeRef<CFNumberRef> cf_max_frame_time(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &max_frame_time));
      base::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
          CGColorSpaceCreateWithName(kCGColorSpaceSRGB));

      const size_t kNumKeys = 3;
      const void* keys[kNumKeys] = {
          kCGDisplayStreamShowCursor,
          kCGDisplayStreamMinimumFrameTime,
          kCGDisplayStreamColorSpace,
      };
      const void* values[kNumKeys] = {
          kCFBooleanFalse,
          cf_max_frame_time.get(),
          cg_color_space.get(),
      };
      properties.reset(CFDictionaryCreate(
          kCFAllocatorDefault, keys, values, kNumKeys,
          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    display_stream_.reset(CGDisplayStreamCreate(
        display_id_, requested_format_.frame_size.width(),
        requested_format_.frame_size.height(),
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, properties, handler));
    if (!display_stream_) {
      client_->OnError(
          media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamCreate,
          FROM_HERE, "CGDisplayStreamCreate failed");
      return;
    }
    CGError error = CGDisplayStreamStart(display_stream_);
    if (error != kCGErrorSuccess) {
      client_->OnError(
          media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamStart,
          FROM_HERE, "CGDisplayStreamStart failed");
      return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       CGDisplayStreamGetRunLoopSource(display_stream_),
                       kCFRunLoopCommonModes);
    client_->OnStarted();
  }
  void StopAndDeAllocate() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    min_frame_rate_enforcement_timer_.reset();
    weak_factory_.InvalidateWeakPtrs();
    if (display_stream_) {
      CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                            CGDisplayStreamGetRunLoopSource(display_stream_),
                            kCFRunLoopCommonModes);
      CGDisplayStreamStop(display_stream_);
    }
    display_stream_.reset();
  }

 private:
  void OnReceivedIOSurfaceFromStream(IOSurfaceRef io_surface) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    last_received_io_surface_.reset(io_surface, base::scoped_policy::RETAIN);

    // Immediately send the new frame to the client.
    SendLastReceivedIOSurfaceToClient();
  }
  void SendLastReceivedIOSurfaceToClient() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Package |last_received_io_surface_| as a GpuMemoryBuffer.
    gfx::GpuMemoryBufferHandle handle;
    handle.id.id = -1;
    handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
    handle.io_surface.reset(last_received_io_surface_,
                            base::scoped_policy::RETAIN);

    const auto now = base::TimeTicks::Now();
    if (first_frame_time_.is_null())
      first_frame_time_ = now;

    client_->OnIncomingCapturedExternalBuffer(
        media::CapturedExternalVideoBuffer(std::move(handle), requested_format_,
                                           gfx::ColorSpace::CreateSRGB()),
        {}, now, now - first_frame_time_);

    // Reset |min_frame_rate_enforcement_timer_|.
    if (!min_frame_rate_enforcement_timer_) {
      min_frame_rate_enforcement_timer_ =
          std::make_unique<base::RepeatingTimer>(
              FROM_HERE, base::TimeDelta::FromSecondsD(1 / min_frame_rate_),
              base::BindRepeating(
                  &DesktopCaptureDeviceMac::SendLastReceivedIOSurfaceToClient,
                  weak_factory_.GetWeakPtr()));
    }
    min_frame_rate_enforcement_timer_->Reset();
  }

  // This class assumes single threaded access.
  THREAD_CHECKER(thread_checker_);

  const CGDirectDisplayID display_id_;

  std::unique_ptr<Client> client_;
  base::ScopedCFTypeRef<CGDisplayStreamRef> display_stream_;
  media::VideoCaptureFormat requested_format_;
  float min_frame_rate_ = 1.f;
  gfx::ScopedInUseIOSurface last_received_io_surface_;

  // The time of the first call to SendLastReceivedIOSurfaceToClient. Used to
  // compute the timestamp of subsequent frames.
  base::TimeTicks first_frame_time_;

  // Timer to enforce |min_frame_rate_| by repeatedly calling
  // SendLastReceivedIOSurfaceToClient.
  // TODO(https://crbug.com/1171127): Remove the need for the capture device
  // to re-submit static content.
  std::unique_ptr<base::RepeatingTimer> min_frame_rate_enforcement_timer_;

  base::WeakPtrFactory<DesktopCaptureDeviceMac> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureDeviceMac);
};

}  // namespace

std::unique_ptr<media::VideoCaptureDevice> CreateDesktopCaptureDeviceMac(
    const DesktopMediaID& source) {
  // DesktopCaptureDeviceMac supports only TYPE_SCREEN.
  // https://crbug.com/1176900
  if (source.type != DesktopMediaID::TYPE_SCREEN)
    return nullptr;

  // DesktopCaptureDeviceMac only supports a single display at a time. It
  // will not stitch desktops together.
  // https://crbug.com/1178360
  if (source.id == webrtc::kFullDesktopScreenId ||
      source.id == webrtc::kInvalidScreenId) {
    return nullptr;
  }

  IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
  IncrementDesktopCaptureCounter(source.audio_share
                                     ? SCREEN_CAPTURER_CREATED_WITH_AUDIO
                                     : SCREEN_CAPTURER_CREATED_WITHOUT_AUDIO);
  return std::make_unique<DesktopCaptureDeviceMac>(source.id);
}

}  // namespace content
