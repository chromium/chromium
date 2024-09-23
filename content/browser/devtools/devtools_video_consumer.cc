// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_video_consumer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "media/base/limits.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/renderers/paint_canvas_video_renderer.h"

namespace content {

namespace {

constexpr base::TimeDelta kDefaultMinCapturePeriod = base::Milliseconds(10);

// Frame size can change every frame.
constexpr base::TimeDelta kDefaultMinPeriod = base::TimeDelta();

// Allow variable aspect ratio.
const bool kDefaultUseFixedAspectRatio = false;

constexpr media::VideoPixelFormat kDefaultPixelFormat =
    media::PIXEL_FORMAT_I420;

// Creates a ClientFrameSinkVideoCapturer via HostFrameSinkManager.
std::unique_ptr<viz::ClientFrameSinkVideoCapturer> CreateCapturer() {
  return GetHostFrameSinkManager()->CreateVideoCapturer();
}

}  // namespace

// static
constexpr gfx::Size DevToolsVideoConsumer::kDefaultMinFrameSize;

// static
constexpr gfx::Size DevToolsVideoConsumer::kDefaultMaxFrameSize;

DevToolsVideoConsumer::DevToolsVideoConsumer(OnFrameCapturedCallback callback)
    : callback_(std::move(callback)),
      min_capture_period_(kDefaultMinCapturePeriod),
      min_frame_size_(kDefaultMinFrameSize),
      max_frame_size_(kDefaultMaxFrameSize),
      pixel_format_(kDefaultPixelFormat) {}

DevToolsVideoConsumer::~DevToolsVideoConsumer() = default;

// static
SkBitmap DevToolsVideoConsumer::GetSkBitmapFromFrame(
    scoped_refptr<media::VideoFrame> frame) {
  media::PaintCanvasVideoRenderer renderer;
  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(frame->visible_rect().width(),
                          frame->visible_rect().height());
  cc::SkiaPaintCanvas canvas(skbitmap);
  renderer.Copy(frame, &canvas, nullptr);
  return skbitmap;
}

void DevToolsVideoConsumer::StartCapture() {
  if (capturer_)
    return;
  InnerStartCapture(CreateCapturer());
}

void DevToolsVideoConsumer::StopCapture() {
  if (!capturer_)
    return;
  capturer_.reset();
}

void DevToolsVideoConsumer::SetFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  if (capturer_) {
    capturer_->ChangeTarget(viz::VideoCaptureTarget(frame_sink_id_));
  }
}

void DevToolsVideoConsumer::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  min_capture_period_ = min_capture_period;
  if (capturer_)
    capturer_->SetMinCapturePeriod(min_capture_period_);
}

void DevToolsVideoConsumer::SetMinAndMaxFrameSize(gfx::Size min_frame_size,
                                                  gfx::Size max_frame_size) {
  DCHECK(IsValidMinAndMaxFrameSize(min_frame_size, max_frame_size));
  min_frame_size_ = min_frame_size;
  max_frame_size_ = max_frame_size;
  if (capturer_) {
    capturer_->SetResolutionConstraints(min_frame_size_, max_frame_size_,
                                        kDefaultUseFixedAspectRatio);
  }
}

void DevToolsVideoConsumer::SetFormat(media::VideoPixelFormat format) {
  pixel_format_ = format;
  if (capturer_) {
    capturer_->SetFormat(pixel_format_);
  }
}

void DevToolsVideoConsumer::InnerStartCapture(
    std::unique_ptr<viz::ClientFrameSinkVideoCapturer> capturer) {
  capturer_ = std::move(capturer);

  // Give |capturer_| the capture parameters.
  capturer_->SetMinCapturePeriod(min_capture_period_);
  capturer_->SetMinSizeChangePeriod(kDefaultMinPeriod);
  capturer_->SetResolutionConstraints(min_frame_size_, max_frame_size_,
                                      kDefaultUseFixedAspectRatio);
  capturer_->SetFormat(pixel_format_);
  if (frame_sink_id_.is_valid())
    capturer_->ChangeTarget(viz::VideoCaptureTarget(frame_sink_id_));
  capturer_->Start(this, viz::mojom::BufferFormatPreference::kDefault);
}

bool DevToolsVideoConsumer::IsValidMinAndMaxFrameSize(
    gfx::Size min_frame_size,
    gfx::Size max_frame_size) {
  // Returns true if
  // 0 < |min_frame_size| <= |max_frame_size| <= media::limits::kMaxDimension.
  return 0 < min_frame_size.width() && 0 < min_frame_size.height() &&
         min_frame_size.width() <= max_frame_size.width() &&
         min_frame_size.height() <= max_frame_size.height() &&
         max_frame_size.width() <= media::limits::kMaxDimension &&
         max_frame_size.height() <= media::limits::kMaxDimension;
}

void DevToolsVideoConsumer::OnFrameCaptured(
    ::media::mojom::VideoBufferHandlePtr data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  CHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();

  // The |data| parameter is not nullable and mojo type mapping for
  // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it is
  // the same type, with null check being equivalent to IsValid() check. Given
  // the above, we should never be able to receive a read only shmem region that
  // is not valid - mojo will enforce it for us.
  DCHECK(shmem_region.IsValid());

  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  base::span<const uint8_t> mapping_memory(mapping);
  if (mapping_memory.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  // Create a media::VideoFrame that wraps the read-only shared memory data.
  // Unfortunately, a deep web of not-const-correct code exists in
  // media::VideoFrame and media::PaintCanvasVideoRenderer (see
  // GetSkBitmapFromFrame() above). So, the pointer's const attribute must be
  // casted away. This is safe since the operating system will page fault if
  // there is any attempt downstream to mutate the data.
  //
  // Setting |frame|'s visible rect equal to |content_rect| so that only the
  // portion of the frame that contains content is used.
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      info->pixel_format, info->coded_size, content_rect, content_rect.size(),
      mapping_memory.data(), mapping_memory.size(), info->timestamp);
  if (!frame) {
    DLOG(ERROR) << "Unable to create VideoFrame wrapper around the shmem.";
    return;
  }
  frame->AddDestructionObserver(base::BindOnce(
      [](media::mojom::VideoBufferHandlePtr data,
         base::ReadOnlySharedMemoryMapping mapping,
         mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
             callbacks) {},
      std::move(data), std::move(mapping), std::move(callbacks)));
  frame->set_metadata(info->metadata);
  frame->set_color_space(info->color_space);

  callback_.Run(std::move(frame));
}

void DevToolsVideoConsumer::OnStopped() {}

}  // namespace content
