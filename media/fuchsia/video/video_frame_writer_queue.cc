// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/video/video_frame_writer_queue.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// Hardcoded constants defined in the Amlogic driver.
// TODO(crbug.com/42050532): Get this values from platform API rather than
// hardcoding them.
constexpr size_t kHeightAlignment = 2;

}  // namespace

VideoFrameWriterQueue::Item::Item(scoped_refptr<VideoFrame> frame,
                                  bool force_keyframe)
    : frame(std::move(frame)), force_keyframe(force_keyframe) {
  DCHECK(this->frame);
}

VideoFrameWriterQueue::Item::Item(Item&&) = default;

VideoFrameWriterQueue::Item::~Item() = default;

VideoFrameWriterQueue::VideoFrameWriterQueue() = default;

VideoFrameWriterQueue::~VideoFrameWriterQueue() = default;

VideoFrameWriterQueue::FrameSize::FrameSize(gfx::Size coded_size,
                                            int dst_y_stride,
                                            int dst_uv_stride,
                                            int dst_y_plane_size,
                                            int dst_uv_plane_size,
                                            int dst_size)
    : coded_size_(coded_size),
      dst_y_stride_(dst_y_stride),
      dst_uv_stride_(dst_uv_stride),
      dst_y_plane_size_(dst_y_plane_size),
      dst_uv_plane_size_(dst_uv_plane_size),
      dst_size_(dst_size) {}

// static
std::optional<VideoFrameWriterQueue::FrameSize>
VideoFrameWriterQueue::FrameSize::Create(
    const fuchsia::sysmem2::ImageFormatConstraints& constraints,
    const gfx::Size& coded_size) {
  if (coded_size.height() % kHeightAlignment != 0) {
    LOG(ERROR) << "Coded height " << coded_size.height()
               << " is not aligned to " << kHeightAlignment;
    return std::nullopt;
  }
  uint32_t divisor = constraints.bytes_per_row_divisor();
  if (divisor == 0) {
    divisor = 1;
  }
  if (divisor % 2 != 0 && divisor != 1) {
    LOG(ERROR) << "Unsupported odd bytes_per_row_divisor: " << divisor;
    return std::nullopt;
  }
  uint32_t stride = std::max(constraints.min_bytes_per_row(),
                             static_cast<uint32_t>(coded_size.width()));
  // The `bytes_per_row_divisor` is not guaranteed to be a power of two
  // because it is negotiated as the Least Common Multiple (LCM) across
  // multiple participants in Sysmem (e.g. hardware decoders/encoders,
  // display controllers). For example, if one participant requires a
  // divisor of 4 (power of 2) and another requires 3 (not a power of 2),
  // the negotiated divisor will be 12. Thus, we cannot use
  // `base::bits::AlignUp` here and must use the generic alignment formula.
  uint64_t dst_y_stride =
      ((static_cast<uint64_t>(stride) + divisor - 1) / divisor) * divisor;
  if (dst_y_stride % 2 != 0) {
    LOG(ERROR) << "Calculated destination Y stride is odd: " << dst_y_stride;
    return std::nullopt;
  }
  if (dst_y_stride == 0) {
    LOG(ERROR) << "Failed to calculate destination Y stride (got 0)";
    return std::nullopt;
  }
  if (dst_y_stride > INT32_MAX) {
    LOG(ERROR) << "Destination Y stride overflowed: " << dst_y_stride;
    return std::nullopt;
  }

  // `dst_y_stride` is guaranteed to be even because:
  // 1. If `divisor` is even, any multiple of it (which `dst_y_stride` is
  //    calculated to be) must be even.
  // 2. If `divisor` is 1 (the only allowed odd divisor), we explicitly
  //    validated and rejected odd `dst_y_stride` above.
  CHECK_EQ(dst_y_stride % 2, 0ull);
  uint64_t uv_stride = (dst_y_stride >> 1);
  CHECK_LE(uv_stride, static_cast<uint64_t>(INT32_MAX));

  uint64_t y_plane_size = uint64_t(coded_size.height()) * dst_y_stride;
  if (y_plane_size > INT32_MAX) {
    LOG(ERROR) << "Y plane size calculation overflowed: " << coded_size.height()
               << " * " << dst_y_stride << " = " << y_plane_size;
    return std::nullopt;
  }

  size_t total_size = y_plane_size;
  total_size += (total_size >> 1);
  if (total_size > INT32_MAX) {
    LOG(ERROR) << "Total size calculation overflowed: " << total_size;
    return std::nullopt;
  }

  return FrameSize(coded_size, static_cast<int>(dst_y_stride),
                   static_cast<int>(uv_stride), static_cast<int>(y_plane_size),
                   static_cast<int>(y_plane_size >> 2),
                   static_cast<int>(total_size));
}

bool VideoFrameWriterQueue::Enqueue(scoped_refptr<VideoFrame> frame,
                                    bool force_keyframe) {
  if (frame->format() != PIXEL_FORMAT_I420) {
    LOG(ERROR) << "Unsupported frame format: " << frame->format();
    return false;
  }

  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  // Frame dimensions must be positive. We now explicitly disallow 0 dimensions
  // (empty frames), which were previously allowed in production (where DCHECKs
  // are disabled) as no-op copies.
  if (width <= 0 || height <= 0) {
    LOG(ERROR) << "Invalid frame dimensions: " << width << "x" << height;
    return false;
  }

  // YUV 4:2:0 formats (like I420) require height to be aligned to 2 for chroma
  // planes. We must explicitly check the input frame height to match the
  // kHeightAlignment to avoid unexpected out-of-bound write in I420Copy (which
  // uses (height + 1) / 2 for UV rows, whereas we allocate buffer assuming
  // height is aligned).
  if (height % kHeightAlignment != 0) {
    LOG(ERROR) << "Input frame height " << height << " is not aligned to "
               << kHeightAlignment;
    return false;
  }

  for (auto plane :
       {VideoFrame::Plane::kY, VideoFrame::Plane::kU, VideoFrame::Plane::kV}) {
    int stride = frame->stride(plane);
    size_t plane_width = VideoFrame::Columns(plane, PIXEL_FORMAT_I420, width);
    if (stride < 0 || static_cast<size_t>(stride) < plane_width) {
      LOG(ERROR) << "Invalid stride for plane " << plane << ": " << stride
                 << " (minimum: " << plane_width << ")";
      return false;
    }
    size_t rows = VideoFrame::Rows(plane, PIXEL_FORMAT_I420, height);
    if (rows > 0) {
      uint64_t required_size = uint64_t(stride) * (rows - 1) + plane_width;
      if (frame->data_span(plane).size() < required_size) {
        LOG(ERROR) << "Data span size too small for plane " << plane << ": "
                   << frame->data_span(plane).size() << " < " << required_size;
        return false;
      }
    }
  }

  queue_.emplace(std::move(frame), force_keyframe);

  if (!buffers_.empty()) {
    ProcessQueue();
  }
  return true;
}

EncoderStatus VideoFrameWriterQueue::Initialize(
    std::vector<VmoBuffer> buffers,
    fuchsia::sysmem2::SingleBufferSettings buffer_settings,
    fuchsia::media::FormatDetails initial_format_details,
    gfx::Size coded_size,
    ProcessCB process_cb) {
  DCHECK(buffers_.empty());
  DCHECK(!buffers.empty());

  buffers_ = std::move(buffers);
  format_details_ = std::move(initial_format_details);
  process_cb_ = std::move(process_cb);

  auto frame_size =
      FrameSize::Create(buffer_settings.image_format_constraints(), coded_size);
  if (!frame_size) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "Failed to calculate frame size"};
  }
  frame_size_.emplace(std::move(*frame_size));

  for (auto& buffer : buffers_) {
    if (frame_size_->dst_size() > buffer.size()) {
      return {
          EncoderStatus::Codes::kEncoderInitializationError,
          base::StringPrintf("VmoBuffer size (%zu) is smaller than required "
                             "destination size (%zu)",
                             buffer.size(), frame_size_->dst_size())};
    }
  }

  // Initially, all buffers are free to use.
  for (size_t i = 0; i < buffers_.size(); i++) {
    free_buffer_indices_.push(i);
  }

  ProcessQueue();
  return EncoderStatus::Codes::kOk;
}

void VideoFrameWriterQueue::ProcessQueue() {
  CHECK(!buffers_.empty());
  CHECK(frame_size_);

  while (!queue_.empty() && !free_buffer_indices_.empty()) {
    Item item = std::move(queue_.front());
    queue_.pop();
    size_t buffer_index = std::move(free_buffer_indices_.front());
    free_buffer_indices_.pop();

    CHECK(CopyFrameToBuffer(item, buffer_index));

    auto packet = StreamProcessorHelper::IoPacket(
        buffer_index, /*offset=*/0, frame_size_->dst_size(),
        item.frame->timestamp(),
        /*unit_end=*/false, /*key_frame=*/false,
        base::BindOnce(&VideoFrameWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), buffer_index));
    if (item.force_keyframe) {
      fuchsia::media::FormatDetails format_details;
      zx_status_t status = format_details_.Clone(&format_details);
      ZX_DCHECK(status == ZX_OK, status) << "Clone FormatDetails";

      format_details.mutable_encoder_settings()->h264().set_force_key_frame(
          true);
      packet.set_format(std::move(format_details));
    }

    process_cb_.Run(std::move(packet));
  }
}

void VideoFrameWriterQueue::ReleaseBuffer(size_t free_buffer_index) {
  DCHECK(!buffers_.empty());

  free_buffer_indices_.push(free_buffer_index);
  ProcessQueue();
}

bool VideoFrameWriterQueue::CopyFrameToBuffer(const Item& item,
                                              size_t buffer_index) {
  if (!frame_size_) [[unlikely]] {
    return false;
  }
  auto& frame = item.frame;
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();

  if (width <= 0 || height <= 0 || width > frame_size_->coded_size().width() ||
      height > frame_size_->coded_size().height()) [[unlikely]] {
    return false;
  }

  if (frame_size_->dst_size() > buffers_[buffer_index].size()) [[unlikely]] {
    return false;
  }

  auto dst_span = buffers_[buffer_index].GetWritableMemory();
  // Strides are guaranteed to be safe by Enqueue validation.

  return libyuv::I420Copy(
             frame->data(VideoFrame::Plane::kY),
             frame->stride(VideoFrame::Plane::kY),
             frame->data(VideoFrame::Plane::kU),
             frame->stride(VideoFrame::Plane::kU),
             frame->data(VideoFrame::Plane::kV),
             frame->stride(VideoFrame::Plane::kV),
             dst_span.first(frame_size_->dst_y_plane_size()).data(),
             frame_size_->dst_y_stride(),
             dst_span
                 .subspan(frame_size_->dst_y_plane_size(),
                          frame_size_->dst_uv_plane_size())
                 .data(),
             frame_size_->dst_uv_stride(),
             dst_span
                 .subspan(frame_size_->dst_y_plane_size() +
                              frame_size_->dst_uv_plane_size(),
                          frame_size_->dst_uv_plane_size())
                 .data(),
             frame_size_->dst_uv_stride(), width, height) == 0;
}

}  // namespace media
