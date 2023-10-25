// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/ash_mojom_video_consumer.h"

#include <cstddef>
#include <memory>

#include "base/notreached.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/geometry/rect.h"

namespace remoting {

namespace {

webrtc::DesktopRect ToDesktopRect(gfx::Rect rect) {
  return webrtc::DesktopRect::MakeLTRB(rect.x(), rect.y(), rect.right(),
                                       rect.bottom());
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
  bool IsValidFrame() const;
  int GetDpi() const;

  media::mojom::VideoFrameInfoPtr info_;
  base::ReadOnlySharedMemoryMapping pixels_;
  gfx::Rect content_rect_;
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
  memcpy(bitmap->getPixels(), pixels_.memory(), bitmap->computeByteSize());

  return bitmap;
}

std::unique_ptr<webrtc::DesktopFrame>
AshMojomVideoConsumer::Frame::ToDesktopFrame(gfx::Point origin) const {
  if (!IsValidFrame()) {
    return nullptr;
  }

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

bool AshMojomVideoConsumer::Frame::IsValidFrame() const {
  if (!info_) {
    // No Frame data present
    return false;
  }

  if (!pixels_.IsValid()) {
    LOG(ERROR) << "Shared memory mapping failed.";
    return false;
  }

  if (pixels_.size() < media::VideoFrame::AllocationSize(info_->pixel_format,
                                                         info_->coded_size)) {
    LOG(ERROR) << "Shared memory size was less than expected.";
    return false;
  }

  return true;
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

  auto updated_rect = info->metadata.capture_update_rect.value_or(content_rect);
  updated_region_aggregator_.AddUpdatedRect(ToDesktopRect(updated_rect));
  updated_region_aggregator_.HandleSizeChange(content_rect.size());

  latest_frame_ =
      std::make_unique<Frame>(std::move(info), shared_memory_region.Map(),
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
// Invoked every time we change target, but, sub_capture_target_version is not
// relevant for window capture.
void AshMojomVideoConsumer::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {}

}  // namespace remoting
