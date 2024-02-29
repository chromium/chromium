// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/io_surface_capture_device_base_mac.h"

#include "media/base/video_util.h"
#include "media/capture/content/capture_resolution_chooser.h"

namespace content {

IOSurfaceCaptureDeviceBase::IOSurfaceCaptureDeviceBase() = default;
IOSurfaceCaptureDeviceBase::~IOSurfaceCaptureDeviceBase() = default;

void IOSurfaceCaptureDeviceBase::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client && !client_);
  client_ = std::move(client);
  capture_params_ = params;

  OnStart();
}

void IOSurfaceCaptureDeviceBase::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_factory_base_.InvalidateWeakPtrs();

  OnStop();
}

void IOSurfaceCaptureDeviceBase::RequestRefreshFrame() {
  // Simply send the last received surface, if we ever received one.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (last_received_io_surface_)
    SendLastReceivedIOSurfaceToClient();
}

void IOSurfaceCaptureDeviceBase::OnReceivedIOSurfaceFromStream(
    gfx::ScopedInUseIOSurface io_surface,
    const media::VideoCaptureFormat& capture_format,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  last_received_io_surface_ = std::move(io_surface);
  last_received_capture_format_ = capture_format;
  last_visible_rect_ = visible_rect;

  // Immediately send the new frame to the client.
  SendLastReceivedIOSurfaceToClient();
}

void IOSurfaceCaptureDeviceBase::SendLastReceivedIOSurfaceToClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Package `last_received_io_surface_` as a GpuMemoryBuffer.
  gfx::GpuMemoryBufferHandle handle;
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  handle.io_surface = last_received_io_surface_;

  const auto now = base::TimeTicks::Now();
  if (first_frame_time_.is_null())
    first_frame_time_ = now;

  client_->OnIncomingCapturedExternalBuffer(
      media::CapturedExternalVideoBuffer(std::move(handle),
                                         last_received_capture_format_,
                                         gfx::ColorSpace::CreateREC709()),
      now, now - first_frame_time_, std::nullopt, last_visible_rect_);
}

void IOSurfaceCaptureDeviceBase::ComputeFrameSizeAndDestRect(
    const gfx::Size& source_size,
    gfx::Size& frame_size,
    gfx::RectF& dest_rect_in_frame) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Compute the destination frame size using CaptureResolutionChooser.
  const auto constraints = capture_params_.SuggestConstraints();
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

  // Compute the rectangle to blit into.
  if (constraints.fixed_aspect_ratio) {
    dest_rect_in_frame = gfx::RectF(media::ComputeLetterboxRegionForI420(
        gfx::Rect(frame_size), source_size));
    // If the target rectangle is not exactly the full frame, then out-set
    // the region by a tiny amount. This works around a bug wherein a green
    // line appears on the left side of the content.
    // https://crbug.com/1267655
    if (dest_rect_in_frame != gfx::RectF(gfx::SizeF(frame_size)))
      dest_rect_in_frame.Outset(1.f / 4096);
  } else {
    // If the aspect ratio is not fixed, then this is the full destination
    // frame.
    dest_rect_in_frame = gfx::RectF(gfx::SizeF(frame_size));
  }
}

}  // namespace content
