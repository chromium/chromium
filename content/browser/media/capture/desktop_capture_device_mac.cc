// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "media/base/video_util.h"
#include "media/capture/content/capture_resolution_chooser.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

namespace {

class DesktopCaptureDeviceMac : public media::VideoCaptureDevice {
 public:
  DesktopCaptureDeviceMac(CGDirectDisplayID display_id)
      : display_id_(display_id),
        device_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        weak_factory_(this) {}

  DesktopCaptureDeviceMac(const DesktopCaptureDeviceMac&) = delete;
  DesktopCaptureDeviceMac& operator=(const DesktopCaptureDeviceMac&) = delete;

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

    base::RepeatingCallback<void(gfx::ScopedInUseIOSurface)>
        received_io_surface_callback = base::BindRepeating(
            &DesktopCaptureDeviceMac::OnReceivedIOSurfaceFromStream,
            weak_factory_.GetWeakPtr());
    CGDisplayStreamFrameAvailableHandler handler =
        ^(CGDisplayStreamFrameStatus status, uint64_t display_time,
          IOSurfaceRef frame_surface, CGDisplayStreamUpdateRef update_ref) {
          gfx::ScopedInUseIOSurface io_surface(frame_surface,
                                               base::scoped_policy::RETAIN);
          if (status == kCGDisplayStreamFrameStatusFrameComplete) {
            device_task_runner_->PostTask(
                FROM_HERE,
                base::BindRepeating(received_io_surface_callback, io_surface));
          }
        };

    // Retrieve the source display's size.
    base::ScopedCFTypeRef<CGDisplayModeRef> mode(
        CGDisplayCopyDisplayMode(display_id_));
    const gfx::Size source_size = mode ? gfx::Size(CGDisplayModeGetWidth(mode),
                                                   CGDisplayModeGetHeight(mode))
                                       : requested_format_.frame_size;

    // Compute the destination frame size using CaptureResolutionChooser.
    const auto constraints = params.SuggestConstraints();
    gfx::Size frame_size = requested_format_.frame_size;
    {
      media::CaptureResolutionChooser resolution_chooser;
      resolution_chooser.SetConstraints(constraints.min_frame_size,
                                        constraints.max_frame_size,
                                        constraints.fixed_aspect_ratio);
      resolution_chooser.SetSourceSize(source_size);
      // Ensure that the resulting frame size has an even width and height. This
      // matches the behavior of DesktopCaptureDevice.
      frame_size = gfx::Size(resolution_chooser.capture_size().width() & ~1,
                             resolution_chooser.capture_size().height() & ~1);
      if (frame_size.IsEmpty())
        frame_size = gfx::Size(2, 2);
    }

    // Compute the rectangle to blit into. If the aspect ratio is not fixed,
    // then this is the full destination frame.
    gfx::RectF dest_rect_in_frame = gfx::RectF(gfx::SizeF(frame_size));
    if (constraints.fixed_aspect_ratio) {
      dest_rect_in_frame = gfx::RectF(media::ComputeLetterboxRegionForI420(
          gfx::Rect(frame_size), source_size));
      // If the target rectangle is not exactly the full frame, then out-set
      // the region by a tiny amount. This works around a bug wherein a green
      // line appears on the left side of the content.
      // https://crbug.com/1267655
      if (dest_rect_in_frame != gfx::RectF(gfx::SizeF(frame_size)))
        dest_rect_in_frame.Outset(1.f / 4096);
    }

    base::ScopedCFTypeRef<CFDictionaryRef> properties;
    {
      float max_frame_time = 1.f / requested_format_.frame_rate;
      base::ScopedCFTypeRef<CFNumberRef> cf_max_frame_time(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &max_frame_time));
      base::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
          CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
      base::ScopedCFTypeRef<CFDictionaryRef> dest_rect_in_frame_dict(
          CGRectCreateDictionaryRepresentation(dest_rect_in_frame.ToCGRect()));

      const size_t kNumKeys = 5;
      const void* keys[kNumKeys] = {
          kCGDisplayStreamShowCursor,       kCGDisplayStreamPreserveAspectRatio,
          kCGDisplayStreamMinimumFrameTime, kCGDisplayStreamColorSpace,
          kCGDisplayStreamDestinationRect,
      };
      const void* values[kNumKeys] = {
          kCFBooleanTrue,
          kCFBooleanFalse,
          cf_max_frame_time.get(),
          cg_color_space.get(),
          dest_rect_in_frame_dict.get(),
      };
      properties.reset(CFDictionaryCreate(
          kCFAllocatorDefault, keys, values, kNumKeys,
          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    requested_format_.frame_size = frame_size;
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
    // Use CFRunLoopGetMain instead of CFRunLoopGetCurrent because in some
    // circumstances (e.g, streaming to ChromeCast), this is called on a
    // worker thread where the CFRunLoop does not get serviced.
    // https://crbug.com/1185388
    CFRunLoopAddSource(CFRunLoopGetMain(),
                       CGDisplayStreamGetRunLoopSource(display_stream_),
                       kCFRunLoopCommonModes);
    client_->OnStarted();
  }
  void StopAndDeAllocate() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    min_frame_rate_enforcement_timer_.reset();
    weak_factory_.InvalidateWeakPtrs();
    if (display_stream_) {
      CFRunLoopRemoveSource(CFRunLoopGetMain(),
                            CGDisplayStreamGetRunLoopSource(display_stream_),
                            kCFRunLoopCommonModes);
      CGDisplayStreamStop(display_stream_);
    }
    display_stream_.reset();
  }

 private:
  void OnReceivedIOSurfaceFromStream(gfx::ScopedInUseIOSurface io_surface) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    last_received_io_surface_ = std::move(io_surface);

    // Immediately send the new frame to the client.
    SendLastReceivedIOSurfaceToClient();
  }
  void SendLastReceivedIOSurfaceToClient() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Package |last_received_io_surface_| as a GpuMemoryBuffer.
    gfx::GpuMemoryBufferHandle handle;
    handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
    handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
    handle.io_surface.reset(last_received_io_surface_,
                            base::scoped_policy::RETAIN);

    const auto now = base::TimeTicks::Now();
    if (first_frame_time_.is_null())
      first_frame_time_ = now;

    client_->OnIncomingCapturedExternalBuffer(
        media::CapturedExternalVideoBuffer(std::move(handle), requested_format_,
                                           gfx::ColorSpace::CreateREC709()),
        {}, now, now - first_frame_time_);

    // Reset |min_frame_rate_enforcement_timer_|.
    if (!min_frame_rate_enforcement_timer_) {
      min_frame_rate_enforcement_timer_ =
          std::make_unique<base::RepeatingTimer>(
              FROM_HERE, base::Seconds(1 / min_frame_rate_),
              base::BindRepeating(
                  &DesktopCaptureDeviceMac::SendLastReceivedIOSurfaceToClient,
                  weak_factory_.GetWeakPtr()));
    }
    min_frame_rate_enforcement_timer_->Reset();
  }

  // This class assumes single threaded access.
  THREAD_CHECKER(thread_checker_);

  const CGDirectDisplayID display_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

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
