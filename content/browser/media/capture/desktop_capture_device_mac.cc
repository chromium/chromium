// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include "base/task/single_thread_task_runner.h"
#include "content/browser/media/capture/io_surface_capture_device_base_mac.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

namespace {

class DesktopCaptureDeviceMac : public IOSurfaceCaptureDeviceBase {
 public:
  DesktopCaptureDeviceMac(CGDirectDisplayID display_id)
      : display_id_(display_id),
        device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        weak_factory_(this) {}

  DesktopCaptureDeviceMac(const DesktopCaptureDeviceMac&) = delete;
  DesktopCaptureDeviceMac& operator=(const DesktopCaptureDeviceMac&) = delete;

  ~DesktopCaptureDeviceMac() override = default;

  // IOSurfaceCaptureDeviceBase:
  void OnStart() override {
    requested_format_ = capture_params().requested_format;
    requested_format_.pixel_format = media::PIXEL_FORMAT_NV12;
    DCHECK_GT(requested_format_.frame_size.GetArea(), 0);
    DCHECK_GT(requested_format_.frame_rate, 0);

    base::RepeatingCallback<void(gfx::ScopedInUseIOSurface)>
        received_io_surface_callback = base::BindRepeating(
            &DesktopCaptureDeviceMac::OnFrame, weak_factory_.GetWeakPtr());
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
    base::apple::ScopedCFTypeRef<CGDisplayModeRef> mode(
        CGDisplayCopyDisplayMode(display_id_));
    const gfx::Size source_size =
        mode ? gfx::Size(CGDisplayModeGetWidth(mode.get()),
                         CGDisplayModeGetHeight(mode.get()))
             : requested_format_.frame_size;

    // Compute the destination frame size using CaptureResolutionChooser.
    gfx::RectF dest_rect_in_frame;
    ComputeFrameSizeAndDestRect(source_size, requested_format_.frame_size,
                                dest_rect_in_frame);

    base::apple::ScopedCFTypeRef<CFDictionaryRef> properties;
    {
      float max_frame_time = 1.f / requested_format_.frame_rate;
      base::apple::ScopedCFTypeRef<CFNumberRef> cf_max_frame_time(
          CFNumberCreate(nullptr, kCFNumberFloat32Type, &max_frame_time));
      base::apple::ScopedCFTypeRef<CGColorSpaceRef> cg_color_space(
          CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
      base::apple::ScopedCFTypeRef<CFDictionaryRef> dest_rect_in_frame_dict(
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

    display_stream_.reset(
        CGDisplayStreamCreate(display_id_, requested_format_.frame_size.width(),
                              requested_format_.frame_size.height(),
                              kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                              properties.get(), handler));
    if (!display_stream_) {
      client()->OnError(
          media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamCreate,
          FROM_HERE, "CGDisplayStreamCreate failed");
      return;
    }
    CGError error = CGDisplayStreamStart(display_stream_.get());
    if (error != kCGErrorSuccess) {
      client()->OnError(
          media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamStart,
          FROM_HERE, "CGDisplayStreamStart failed");
      return;
    }
    // Use CFRunLoopGetMain instead of CFRunLoopGetCurrent because in some
    // circumstances (e.g, streaming to ChromeCast), this is called on a
    // worker thread where the CFRunLoop does not get serviced.
    // https://crbug.com/1185388
    CFRunLoopAddSource(CFRunLoopGetMain(),
                       CGDisplayStreamGetRunLoopSource(display_stream_.get()),
                       kCFRunLoopCommonModes);
    client()->OnStarted();
  }
  void OnStop() override {
    weak_factory_.InvalidateWeakPtrs();
    if (display_stream_) {
      CFRunLoopRemoveSource(
          CFRunLoopGetMain(),
          CGDisplayStreamGetRunLoopSource(display_stream_.get()),
          kCFRunLoopCommonModes);
      CGDisplayStreamStop(display_stream_.get());
    }
    display_stream_.reset();
  }

 private:
  void OnFrame(gfx::ScopedInUseIOSurface io_surface) {
    OnReceivedIOSurfaceFromStream(io_surface, requested_format_,
                                  gfx::Rect(requested_format_.frame_size));
  }

  const CGDirectDisplayID display_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  base::apple::ScopedCFTypeRef<CGDisplayStreamRef> display_stream_;
  media::VideoCaptureFormat requested_format_;
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
