// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/ash_mojom_video_consumer.h"

#include <cstddef>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/geometry/rect.h"

namespace remoting {

namespace {

// We only support N32 SkBitmaps (e.g., ARGB), which are always 4 bytes per
// pixel.
constexpr size_t kBytesPerPixel = 4;

webrtc::DesktopRect ToDesktopRect(gfx::Rect rect) {
  return webrtc::DesktopRect::MakeLTRB(rect.x(), rect.y(), rect.right(),
                                       rect.bottom());
}

std::optional<size_t> GetStride(const media::mojom::VideoFrameInfoPtr& info) {
  // This may be called before the format is fully validated, but in
  // IsValidFrame it is called after basic format checks, making it safe for
  // the supported formats.
  if (info->strides) {
    if (info->strides->stride_by_plane.empty()) {
      return std::nullopt;
    }
    return info->strides->stride_by_plane[0];
  }
  return media::VideoFrame::RowBytes(0, info->pixel_format,
                                     info->coded_size.width());
}

bool IsValidFrame(const media::mojom::VideoFrameInfoPtr& info,
                  const base::ReadOnlySharedMemoryMapping& pixels,
                  const gfx::Rect& content_rect) {
  if (!info) {
    return false;
  }

  if (info->coded_size.IsEmpty()) {
    VLOG(1) << "coded_size is empty.";
    return false;
  }

  if (!pixels.IsValid()) {
    LOG(ERROR) << "Shared memory mapping failed.";
    return false;
  }

  // We only support single-plane N32 SkBitmaps (e.g., ARGB), which are always
  // 4 bytes per pixel.
  if (media::VideoFrame::NumPlanes(info->pixel_format) != 1 ||
      static_cast<size_t>(media::VideoFrame::BytesPerElement(
          info->pixel_format, 0)) != kBytesPerPixel) {
    VLOG(1) << "Unsupported pixel format: " << info->pixel_format;
    return false;
  }

  std::optional<size_t> stride = GetStride(info);
  if (!stride) {
    VLOG(1) << "Invalid or missing stride information.";
    return false;
  }

  // Ensure the stride is at least the width of the frame.
  base::CheckedNumeric<size_t> min_stride = info->coded_size.width();
  min_stride *= kBytesPerPixel;
  size_t valid_min_stride;
  if (!min_stride.AssignIfValid(&valid_min_stride) ||
      *stride < valid_min_stride) {
    VLOG(1) << "Invalid stride: " << *stride;
    return false;
  }

  // Ensure the content rect is within the bounds of the coded size.
  // gfx::Rect(size) creates a rect at (0,0) with given dimensions.
  // Contains() will return false if content_rect has negative offsets or
  // dimensions that exceed the coded size. Note that gfx::Rect enforces
  // non-negative dimensions at construction.
  if (!gfx::Rect(info->coded_size).Contains(content_rect)) {
    VLOG(1) << "content_rect is out of bounds of coded_size.";
    return false;
  }

  // Calculating the minimum buffer size needed to read all rows up to the last
  // pixel of the last row.
  base::CheckedNumeric<size_t> min_required_size = 0;
  if (info->coded_size.height() > 0) {
    min_required_size = info->coded_size.height() - 1;
    min_required_size *= *stride;
    min_required_size += valid_min_stride;
  }

  size_t valid_min_required_size;
  if (!min_required_size.AssignIfValid(&valid_min_required_size)) {
    VLOG(1) << "Calculated minimum required size is invalid (overflow).";
    return false;
  }

  if (pixels.size() < valid_min_required_size) {
    VLOG(1) << "Shared memory size was less than expected. Expected "
            << valid_min_required_size << " got " << pixels.size();
    return false;
  }

  return true;
}

}  // namespace

AshMojomVideoConsumer::UpdatedRegionAggregator::UpdatedRegionAggregator() =
    default;
AshMojomVideoConsumer::UpdatedRegionAggregator::~UpdatedRegionAggregator() =
    default;

webrtc::DesktopRegion
AshMojomVideoConsumer::UpdatedRegionAggregator::TakeUpdatedRegion() {
  webrtc::DesktopRegion desktop_region{desktop_region_};
  desktop_region_.Clear();
  return desktop_region;
}

void AshMojomVideoConsumer::UpdatedRegionAggregator::AddUpdatedRect(
    webrtc::DesktopRect updated_rect) {
  desktop_region_.AddRect(updated_rect);
}

void AshMojomVideoConsumer::UpdatedRegionAggregator::HandleSizeChange(
    gfx::Size new_size) {
  if (new_size == current_frame_size_) {
    return;
  }

  current_frame_size_ = new_size;
  // desktop_region_ must be cleared to make sure no aggregated updated_rect of
  // a different frame size is used. Not clearing it will lead to crashes.
  // desktop_region_ is reset to DesktopRegion of the new size.
  desktop_region_.SetRect(webrtc::DesktopRect::MakeWH(
      current_frame_size_.width(), current_frame_size_.height()));
}

class AshMojomVideoConsumer::Frame {
 public:
  Frame();
  Frame(media::mojom::VideoFrameInfoPtr info,
        base::ReadOnlySharedMemoryMapping pixels,
        gfx::Rect content_rect,
        mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
            done_callback);
  Frame(Frame&&);
  Frame& operator=(Frame&&);
  ~Frame();

  std::unique_ptr<webrtc::DesktopFrame> ToDesktopFrame(gfx::Point origin) const;

 private:
  std::unique_ptr<SkBitmap> CreateSkBitmap() const;
  int GetDpi() const;

  media::mojom::VideoFrameInfoPtr info_;
  base::ReadOnlySharedMemoryMapping pixels_;
  gfx::Rect content_rect_;
  size_t stride_;
  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      done_callback_remote_;
};

AshMojomVideoConsumer::Frame::Frame(Frame&&) = default;
AshMojomVideoConsumer::Frame& AshMojomVideoConsumer::Frame::operator=(Frame&&) =
    default;

AshMojomVideoConsumer::Frame::Frame(
    media::mojom::VideoFrameInfoPtr info,
    base::ReadOnlySharedMemoryMapping pixels,
    gfx::Rect content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        done_callback_remote)
    : info_(std::move(info)),
      pixels_(std::move(pixels)),
      content_rect_(content_rect),
      stride_(GetStride(info_).value()),
      done_callback_remote_(std::move(done_callback_remote)) {}

AshMojomVideoConsumer::Frame::~Frame() {
  done_callback_remote_->Done();
}

std::unique_ptr<SkBitmap> AshMojomVideoConsumer::Frame::CreateSkBitmap() const {
  auto bitmap = std::make_unique<SkBitmap>();
  auto size = content_rect_.size();

  bitmap->allocPixels(
      SkImageInfo::MakeN32(size.width(), size.height(), kOpaque_SkAlphaType,
                           info_->color_space.ToSkColorSpace()));
  CHECK(bitmap->getPixels());

  size_t src_stride = stride_;
  size_t dest_stride = bitmap->rowBytes();
  size_t row_bytes = size.width() * kBytesPerPixel;

  // SAFETY: Both the source and destination buffer sizes have been validated in
  // OnFrameCaptured via IsValidFrame().
  auto src_span = UNSAFE_BUFFERS(base::span(
      static_cast<const uint8_t*>(pixels_.memory()), pixels_.size()));
  auto dest_span = UNSAFE_BUFFERS(base::span(
      static_cast<uint8_t*>(bitmap->getPixels()), bitmap->computeByteSize()));

  for (int y = 0; y < size.height(); ++y) {
    size_t src_row_offset = (content_rect_.y() + y) * src_stride +
                            content_rect_.x() * kBytesPerPixel;
    size_t dest_row_offset = y * dest_stride;

    dest_span.subspan(dest_row_offset, row_bytes)
        .copy_from(src_span.subspan(src_row_offset, row_bytes));
  }

  return bitmap;
}

std::unique_ptr<webrtc::DesktopFrame>
AshMojomVideoConsumer::Frame::ToDesktopFrame(gfx::Point origin) const {
  std::unique_ptr<webrtc::DesktopFrame> frame(
      SkiaBitmapDesktopFrame::Create(CreateSkBitmap()));
  frame->set_top_left(webrtc::DesktopVector(origin.x(), origin.y()));
  frame->set_dpi(webrtc::DesktopVector(GetDpi(), GetDpi()));

  return frame;
}

int AshMojomVideoConsumer::Frame::GetDpi() const {
  return AshProxy::ScaleFactorToDpi(
      info_->metadata.device_scale_factor.value_or(1));
}

AshMojomVideoConsumer::AshMojomVideoConsumer() = default;
AshMojomVideoConsumer::~AshMojomVideoConsumer() = default;

mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer>
AshMojomVideoConsumer::Bind() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

std::unique_ptr<webrtc::DesktopFrame> AshMojomVideoConsumer::GetLatestFrame(
    gfx::Point origin) {
  if (!latest_frame_) {
    return nullptr;
  }

  auto desktop_frame = latest_frame_->ToDesktopFrame(origin);
  if (!desktop_frame) {
    return nullptr;
  }

  desktop_frame->mutable_updated_region()->AddRegion(
      updated_region_aggregator_.TakeUpdatedRegion());

  return desktop_frame;
}

void AshMojomVideoConsumer::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data->get_read_only_shmem_region();

  DCHECK(shared_memory_region.IsValid());
  base::ReadOnlySharedMemoryMapping mapping = shared_memory_region.Map();

  if (!IsValidFrame(info, mapping, content_rect)) {
    mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>(
        std::move(callbacks))
        ->Done();
    return;
  }

  auto updated_rect = info->metadata.capture_update_rect.value_or(content_rect);

  // Ensure the capture_update_rect is within the bounds of the content_rect.
  if (!content_rect.Contains(updated_rect)) {
    VLOG(1) << "capture_update_rect is out of bounds of content_rect.";
    mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>(
        std::move(callbacks))
        ->Done();
    return;
  }

  updated_region_aggregator_.AddUpdatedRect(ToDesktopRect(updated_rect));
  updated_region_aggregator_.HandleSizeChange(content_rect.size());

  latest_frame_ = std::make_unique<Frame>(std::move(info), std::move(mapping),
                                          content_rect, std::move(callbacks));
}

void AshMojomVideoConsumer::OnFrameWithEmptyRegionCapture() {
  // This method is not invoked when capturing entire desktops.
  NOTREACHED();
}

void AshMojomVideoConsumer::OnStopped() {
  receiver_.reset();
  // release the data of the last received frame.
  latest_frame_ = nullptr;
}

void AshMojomVideoConsumer::OnLog(const std::string& message) {
  VLOG(3) << "AshMojomVideoConsumer::OnLog : " << message;
}
// Invoked every time we change target, but `capture_version` is not
// relevant for window capture.
void AshMojomVideoConsumer::OnNewCaptureVersion(
    const media::CaptureVersion& capture_version) {}

}  // namespace remoting
