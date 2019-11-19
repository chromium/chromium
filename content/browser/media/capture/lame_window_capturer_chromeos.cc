// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/lame_window_capturer_chromeos.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/geometry/rect.h"

using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {
// Returns |raw_size| with width and height truncated to even-numbered values.
gfx::Size AdjustSizeForI420Format(const gfx::Size& raw_size) {
  return gfx::Size(raw_size.width() & ~1, raw_size.height() & ~1);
}
}  // namespace

// static
constexpr base::TimeDelta LameWindowCapturerChromeOS::kAbsoluteMinCapturePeriod;

LameWindowCapturerChromeOS::LameWindowCapturerChromeOS(aura::Window* target)
    : target_(target), copy_request_source_(base::UnguessableToken::Create()) {
  if (target_) {
    target_->AddObserver(this);
  }
}

LameWindowCapturerChromeOS::~LameWindowCapturerChromeOS() {
  if (target_) {
    target_->RemoveObserver(this);
  }
}

void LameWindowCapturerChromeOS::SetFormat(media::VideoPixelFormat format,
                                           const gfx::ColorSpace& color_space) {
  if (format != media::PIXEL_FORMAT_I420) {
    LOG(DFATAL) << "Invalid pixel format: Only I420 is supported.";
  }

  if (color_space.IsValid() && color_space != gfx::ColorSpace::CreateREC709()) {
    LOG(DFATAL) << "Unsupported color space: Only BT.709 is supported.";
  }
}

void LameWindowCapturerChromeOS::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  capture_period_ = std::max(min_capture_period, kAbsoluteMinCapturePeriod);

  // If the capture period is being changed while the timer is already running,
  // re-start with the new capture period.
  if (timer_.IsRunning()) {
    timer_.Start(FROM_HERE, capture_period_, this,
                 &LameWindowCapturerChromeOS::CaptureNextFrame);
  }
}

void LameWindowCapturerChromeOS::SetMinSizeChangePeriod(
    base::TimeDelta min_period) {}

void LameWindowCapturerChromeOS::SetResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  if (max_size.width() <= 1 || max_size.height() <= 1 ||
      max_size.width() > media::limits::kMaxDimension ||
      max_size.height() > media::limits::kMaxDimension) {
    LOG(DFATAL) << "Invalid max_size (" << max_size.ToString()
                << "): It must be within media::limits.";
    return;
  }

  // Set the capture size to the max size, adjusted for the I420 format.
  capture_size_ = AdjustSizeForI420Format(max_size);
  DCHECK(!capture_size_.IsEmpty());

  // Cancel any in-flight captures that would be using the old size and clear
  // the buffer pool.
  weak_factory_.InvalidateWeakPtrs();
  buffer_pool_.clear();
  in_flight_count_ = 0;
}

void LameWindowCapturerChromeOS::SetAutoThrottlingEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void LameWindowCapturerChromeOS::ChangeTarget(
    const base::Optional<viz::FrameSinkId>& frame_sink_id) {
  // The LameWindowCapturerChromeOS does not capture from compositor frame
  // sinks.
}

void LameWindowCapturerChromeOS::Start(
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer) {
  DCHECK(consumer);

  Stop();

  consumer_.Bind(std::move(consumer));
  // In the future, if the connection to the consumer is lost before a call to
  // Stop(), make that call on its behalf.
  consumer_.set_disconnect_handler(base::BindOnce(
      &LameWindowCapturerChromeOS::Stop, base::Unretained(this)));

  timer_.Start(FROM_HERE, capture_period_, this,
               &LameWindowCapturerChromeOS::CaptureNextFrame);
}

void LameWindowCapturerChromeOS::Stop() {
  // Stop the timer, cancel any in-flight frames, and clear the buffer pool.
  timer_.Stop();
  weak_factory_.InvalidateWeakPtrs();
  buffer_pool_.clear();
  in_flight_count_ = 0;

  if (consumer_) {
    consumer_->OnStopped();
    consumer_.reset();
  }
}

void LameWindowCapturerChromeOS::RequestRefreshFrame() {
  // This is ignored because the LameWindowCapturerChromeOS captures frames
  // continuously.
}

void LameWindowCapturerChromeOS::CreateOverlay(
    int32_t stacking_index,
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver) {
  // LameWindowCapturerChromeOS only supports one overlay at a time. If one
  // already exists, the following will cause it to be dropped.
  overlay_ =
      std::make_unique<LameCaptureOverlayChromeOS>(this, std::move(receiver));
}

class LameWindowCapturerChromeOS::InFlightFrame
    : public viz::mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  InFlightFrame(base::WeakPtr<LameWindowCapturerChromeOS> capturer,
                base::MappedReadOnlyRegion buffer)
      : capturer_(std::move(capturer)), buffer_(std::move(buffer)) {}

  ~InFlightFrame() final { Done(); }

  base::ReadOnlySharedMemoryRegion CloneBufferHandle() {
    return buffer_.region.Duplicate();
  }

  VideoFrame* video_frame() const { return video_frame_.get(); }
  void set_video_frame(scoped_refptr<VideoFrame> frame) {
    video_frame_ = std::move(frame);
  }

  const gfx::Rect& content_rect() const { return content_rect_; }
  void set_content_rect(const gfx::Rect& rect) { content_rect_ = rect; }

  void set_overlay_renderer(LameCaptureOverlayChromeOS::OnceRenderer renderer) {
    overlay_renderer_ = std::move(renderer);
  }
  void RenderOptionalOverlay() {
    if (overlay_renderer_) {
      std::move(overlay_renderer_).Run(video_frame_.get());
    }
  }

  void Done() final {
    video_frame_ = nullptr;

    if (auto* capturer = capturer_.get()) {
      DCHECK_GT(capturer->in_flight_count_, 0);
      --capturer->in_flight_count_;
      // If the capture size hasn't changed, return the buffer to the pool.
      if (buffer_.mapping.size() ==
          VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                     capturer->capture_size_)) {
        capturer->buffer_pool_.emplace_back(std::move(buffer_));
      }
      capturer_ = nullptr;
    }

    buffer_ = base::MappedReadOnlyRegion();
  }

  void ProvideFeedback(double utilization) final {}

 private:
  base::WeakPtr<LameWindowCapturerChromeOS> capturer_;
  base::MappedReadOnlyRegion buffer_;
  scoped_refptr<VideoFrame> video_frame_;
  gfx::Rect content_rect_;
  LameCaptureOverlayChromeOS::OnceRenderer overlay_renderer_;

  DISALLOW_COPY_AND_ASSIGN(InFlightFrame);
};

void LameWindowCapturerChromeOS::OnOverlayConnectionLost(
    LameCaptureOverlayChromeOS* overlay) {
  if (overlay_.get() == overlay) {
    overlay_.reset();
  }
}

void LameWindowCapturerChromeOS::CaptureNextFrame() {
  // If the maximum frame in-flight count has been reached, skip this frame.
  if (in_flight_count_ >= kMaxFramesInFlight) {
    return;
  }

  // Attempt to re-use a buffer from the pool. Otherwise, create a new one.
  const size_t allocation_size =
      VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, capture_size_);
  base::MappedReadOnlyRegion buffer;
  if (buffer_pool_.empty()) {
    buffer = mojo::CreateReadOnlySharedMemoryRegion(allocation_size);
    if (!buffer.IsValid()) {
      // If the shared memory region creation failed, just abort this frame,
      // hoping the issue is a transient one (e.g., lack of an available region
      // in the address space).
      return;
    }
  } else {
    buffer = std::move(buffer_pool_.back());
    buffer_pool_.pop_back();
    DCHECK(buffer.IsValid());
    DCHECK_EQ(buffer.mapping.size(), allocation_size);
  }
  void* const backing_memory = buffer.mapping.memory();

  // At this point, frame capture will proceed. Create an InFlightFrame to track
  // population and consumption of the frame, and to eventually return the
  // buffer to the pool and decrement |in_flight_count_|.
  ++in_flight_count_;
  auto in_flight_frame = std::make_unique<InFlightFrame>(
      weak_factory_.GetWeakPtr(), std::move(buffer));

  // Create a VideoFrame that wraps the mapped buffer.
  const base::TimeTicks begin_time = base::TimeTicks::Now();
  if (first_frame_reference_time_.is_null()) {
    first_frame_reference_time_ = begin_time;
  }
  in_flight_frame->set_video_frame(VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_I420, capture_size_, gfx::Rect(capture_size_),
      capture_size_, static_cast<uint8_t*>(backing_memory), allocation_size,
      begin_time - first_frame_reference_time_));
  auto* const frame = in_flight_frame->video_frame();
  DCHECK(frame);
  VideoFrameMetadata* const metadata = frame->metadata();
  metadata->SetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME, begin_time);
  metadata->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION, capture_period_);
  metadata->SetDouble(VideoFrameMetadata::FRAME_RATE,
                      1.0 / capture_period_.InSecondsF());
  metadata->SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME, begin_time);
  frame->set_color_space(gfx::ColorSpace::CreateREC709());

  // Compute the region of the VideoFrame that will contain the content. If
  // there is nothing to copy from/to (e.g., the target is gone, or is sized too
  // small), send a blank black frame immediately.
  const gfx::Size source_size =
      target_ ? target_->bounds().size() : gfx::Size();
  const gfx::Rect content_rect = source_size.IsEmpty()
                                     ? gfx::Rect()
                                     : media::ComputeLetterboxRegionForI420(
                                           frame->visible_rect(), source_size);
  in_flight_frame->set_content_rect(content_rect);
  if (content_rect.IsEmpty()) {
    media::LetterboxVideoFrame(frame, gfx::Rect());
    DeliverFrame(std::move(in_flight_frame));
    return;
  }
  DCHECK(target_);

  if (overlay_) {
    in_flight_frame->set_overlay_renderer(overlay_->MakeRenderer(content_rect));
  }

  // Request a copy of the Layer associated with the |target_| aura::Window.
  auto request = std::make_unique<viz::CopyOutputRequest>(
      // Note: As of this writing, I420_PLANES is not supported external to VIZ.
      viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&LameWindowCapturerChromeOS::DidCopyFrame,
                     weak_factory_.GetWeakPtr(), std::move(in_flight_frame)));
  request->set_source(copy_request_source_);
  request->set_area(gfx::Rect(source_size));
  request->SetScaleRatio(
      gfx::Vector2d(source_size.width(), source_size.height()),
      gfx::Vector2d(content_rect.width(), content_rect.height()));
  request->set_result_selection(gfx::Rect(content_rect.size()));
  target_->layer()->RequestCopyOfOutput(std::move(request));
}

void LameWindowCapturerChromeOS::DidCopyFrame(
    std::unique_ptr<InFlightFrame> in_flight_frame,
    std::unique_ptr<viz::CopyOutputResult> result) {
  // Populate the VideoFrame from the CopyOutputResult.
  auto* const frame = in_flight_frame->video_frame();
  DCHECK(frame);
  const auto& content_rect = in_flight_frame->content_rect();
  const int y_stride = frame->stride(VideoFrame::kYPlane);
  uint8_t* const y = frame->visible_data(VideoFrame::kYPlane) +
                     content_rect.y() * y_stride + content_rect.x();
  const int u_stride = frame->stride(VideoFrame::kUPlane);
  uint8_t* const u = frame->visible_data(VideoFrame::kUPlane) +
                     (content_rect.y() / 2) * u_stride + (content_rect.x() / 2);
  const int v_stride = frame->stride(VideoFrame::kVPlane);
  uint8_t* const v = frame->visible_data(VideoFrame::kVPlane) +
                     (content_rect.y() / 2) * v_stride + (content_rect.x() / 2);
  if (!result->ReadI420Planes(y, y_stride, u, u_stride, v, v_stride)) {
    return;  // Copy request failed, punt.
  }

  in_flight_frame->RenderOptionalOverlay();

  // The result may be smaller than what was requested, if unforeseen clamping
  // to the source boundaries occurred by the executor of the copy request.
  // However, the result should never contain more than what was requested.
  DCHECK_LE(result->size().width(), content_rect.width());
  DCHECK_LE(result->size().height(), content_rect.height());
  media::LetterboxVideoFrame(
      frame, gfx::Rect(content_rect.origin(),
                       AdjustSizeForI420Format(result->size())));

  DeliverFrame(std::move(in_flight_frame));
}

void LameWindowCapturerChromeOS::DeliverFrame(
    std::unique_ptr<InFlightFrame> in_flight_frame) {
  auto* const frame = in_flight_frame->video_frame();
  DCHECK(frame);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                  base::TimeTicks::Now());

  // Clone the buffer handle for the consumer.
  base::ReadOnlySharedMemoryRegion handle =
      in_flight_frame->CloneBufferHandle();
  if (!handle.IsValid()) {
    return;  // This should only fail if the OS is exhausted of handles.
  }

  // Assemble frame layout, format, and metadata into a mojo struct to send to
  // the consumer.
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
  info->timestamp = frame->timestamp();
  info->metadata = frame->metadata()->GetInternalValues().Clone();
  info->pixel_format = frame->format();
  info->coded_size = frame->coded_size();
  info->visible_rect = frame->visible_rect();
  DCHECK(frame->ColorSpace().IsValid());  // Ensure it was set by this point.
  info->color_space = frame->ColorSpace();
  const gfx::Rect content_rect = in_flight_frame->content_rect();

  // Create a mojo message pipe and bind to the InFlightFrame to wait for the
  // Done() signal from the consumer. The mojo::SelfOwnedReceiver takes
  // ownership of the InFlightFrame.
  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks;
  mojo::MakeSelfOwnedReceiver(std::move(in_flight_frame),
                              callbacks.InitWithNewPipeAndPassReceiver());

  // Send the frame to the consumer.
  consumer_->OnFrameCaptured(std::move(handle), std::move(info), content_rect,
                             std::move(callbacks));
}

void LameWindowCapturerChromeOS::OnWindowDestroying(aura::Window* window) {
  if (window == target_) {
    target_->RemoveObserver(this);
    target_ = nullptr;
    // The capturer may continue running, but it will notice the target is gone
    // and produce blank black frames hereafter.
  }
}

}  // namespace content
