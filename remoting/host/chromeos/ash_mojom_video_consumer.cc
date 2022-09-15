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

namespace remoting {
class AshMojomVideoConsumer::Frame {
 public:
  Frame();
  Frame(
      media::mojom::VideoFrameInfoPtr info,
      base::ReadOnlySharedMemoryMapping pixels,
      gfx::Rect content_rect,
      mojo::PendingRemote<FrameSinkVideoConsumerFrameCallbacks> done_callback);
  Frame(Frame&&);
  Frame& operator=(Frame&&);
  ~Frame();

  std::unique_ptr<webrtc::DesktopFrame> ToDesktopFrame() const;

 private:
  std::unique_ptr<SkBitmap> CreateSkBitmap() const;
  bool IsValidFrame() const;
  webrtc::DesktopRect GetUpdatedRect() const;
  int GetDpi() const;

  media::mojom::VideoFrameInfoPtr info_;
  base::ReadOnlySharedMemoryMapping pixels_;
  gfx::Rect content_rect_;
  mojo::Remote<FrameSinkVideoConsumerFrameCallbacks> done_callback_remote_;
};

AshMojomVideoConsumer::Frame::Frame(Frame&&) = default;
AshMojomVideoConsumer::Frame& AshMojomVideoConsumer::Frame::operator=(Frame&&) =
    default;

AshMojomVideoConsumer::Frame::Frame(
    media::mojom::VideoFrameInfoPtr info,
    base::ReadOnlySharedMemoryMapping pixels,
    gfx::Rect content_rect,
    mojo::PendingRemote<FrameSinkVideoConsumerFrameCallbacks>
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
                           info_->color_space->ToSkColorSpace()));
  memcpy(bitmap->getPixels(), pixels_.memory(), bitmap->computeByteSize());

  return bitmap;
}

std::unique_ptr<webrtc::DesktopFrame>
AshMojomVideoConsumer::Frame::ToDesktopFrame() const {
  if (!IsValidFrame()) {
    return nullptr;
  }
  int dpi = GetDpi();
  std::unique_ptr<webrtc::DesktopFrame> frame(
      SkiaBitmapDesktopFrame::Create(CreateSkBitmap()));

  frame->set_dpi(webrtc::DesktopVector(dpi, dpi));
  frame->mutable_updated_region()->SetRect(GetUpdatedRect());

  return frame;
}

webrtc::DesktopRect AshMojomVideoConsumer::Frame::GetUpdatedRect() const {
  auto updated_rect =
      info_->metadata.capture_update_rect.value_or(content_rect_);
  return webrtc::DesktopRect::MakeLTRB(updated_rect.x(), updated_rect.y(),
                                       updated_rect.right(),
                                       updated_rect.bottom());
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

  if (!info_->color_space) {
    LOG(ERROR) << "Missing mandatory color space info.";
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

std::unique_ptr<webrtc::DesktopFrame> AshMojomVideoConsumer::GetLatestFrame() {
  if (!latest_frame_) {
    return nullptr;
  }

  return latest_frame_->ToDesktopFrame();
}

void AshMojomVideoConsumer::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<FrameSinkVideoConsumerFrameCallbacks> callbacks) {
  DCHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data->get_read_only_shmem_region();

  DCHECK(shared_memory_region.IsValid());

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
// Invoked every time we change target, but, crop_version is not relevant for
// window capture.
void AshMojomVideoConsumer::OnNewCropVersion(uint32_t crop_version) {}

}  // namespace remoting
