// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/native_pixmap_frame_resource.h"

#include "atomic"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "media/base/format_utils.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/switches.h"

namespace media {

namespace {
gfx::GenericSharedMemoryId GetNextSharedMemoryId() {
  // This uses the same ID generator that is used for creating ID's for GPU
  // memory buffers. Doing so avoids overlapping ID's. No cast is necessary
  // since gfx::GpuMemoryBufferId is an alias of gfx::GenericSharedMemoryId.
  return GetNextGpuMemoryBufferId();
}

// IsValidSize() performs size validity checks similar to those in
// VideoFrame::IsValidConfigInternal().
bool IsValidSize(const gfx::Size& coded_size,
                 const gfx::Rect& visible_rect,
                 const gfx::Size& natural_size) {
  // Checks maximum limits
  if (!VideoFrame::IsValidSize(coded_size, visible_rect, natural_size)) {
    DLOGF(ERROR) << " Invalid size. coded_size:" << coded_size.ToString()
                 << " visible_rect:" << visible_rect.ToString()
                 << " natural_size:" << natural_size.ToString();
    return false;
  }

  // Check that buffer sizes are not empty.
  if (coded_size.IsEmpty()) {
    DLOGF(ERROR) << " Invalid size. coded_size must not be empty";
    return false;
  }
  if (visible_rect.IsEmpty()) {
    DLOGF(ERROR) << " Invalid size. visible_rect must not be empty";
    return false;
  }
  if (natural_size.IsEmpty()) {
    DLOGF(ERROR) << " Invalid size. natural_size must not be empty";
    return false;
  }
  return true;
}

}  // namespace

scoped_refptr<NativePixmapFrameResource> NativePixmapFrameResource::Create(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
  if (!IsValidSize(coded_size, visible_rect, natural_size)) {
    return nullptr;
  }
  // This uses the platform frame utils to allocate a GpuMemoryBufferHandle. The
  // allocated |gmb_handle.native_pixmap_handle| will be moved to the
  // constructed NativePixmapFrameResource.
  auto gmb_handle =
      AllocateGpuMemoryBufferHandle(pixel_format, coded_size, buffer_usage);
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP) {
    DLOGF(ERROR) << "Unable to allocate buffer";
    return nullptr;
  }

  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  // Using CHECK() here is fine. AllocateGpuMemoryBufferHandle() won't return a
  // gfx::GpuMemoryBufferHandle if this conversion fails.
  CHECK(buffer_format.has_value());

  return Create(visible_rect, natural_size, timestamp, buffer_usage,
                base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
                    coded_size, *buffer_format,
                    std::move(gmb_handle.native_pixmap_handle)));
}

scoped_refptr<NativePixmapFrameResource> NativePixmapFrameResource::Create(
    const media::VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::vector<base::ScopedFD> dmabuf_fds,
    base::TimeDelta timestamp) {
  // Performs a sanity check that the number of planes matches the number of
  // file descriptors.
  if (dmabuf_fds.size() != layout.num_planes()) {
    DLOGF(ERROR) << "Layout num_planes=" << layout.num_planes()
                 << "must match dmabuf_fds.size()=" << dmabuf_fds.size();
    return nullptr;
  }

  if (!IsValidSize(layout.coded_size(), visible_rect, natural_size)) {
    return nullptr;
  }

  // This converts |layout|'s VideoPixelFormat to a gfx::BufferFormat, which is
  // needed by the NativePixmapFrameResource constructor.
  auto buffer_format = VideoPixelFormatToGfxBufferFormat(layout.format());
  if (!buffer_format) {
    DLOGF(ERROR) << " Unable to convert pixel format "
                 << VideoPixelFormatToString(layout.format())
                 << " to BufferFormat";
    return nullptr;
  }

  // A gfx::NativePixmapHandle is needed by NativePixmapFrameResource's
  // constructor. This builds one from |layout| and |dmabuf_fds|. The ownership
  // of the FD's in |dmabuf_fds| is transferred to |handle|.
  gfx::NativePixmapHandle handle;
  const size_t num_planes = layout.num_planes();
  handle.planes.reserve(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    const auto& plane = layout.planes()[i];
    handle.planes.emplace_back(plane.stride, plane.offset, plane.size,
                               std::move(dmabuf_fds[i]));
  }

  // This is only ever called with V4L2-allocated buffers, so |layout.modifier|
  // is expected to be kNoModifier.
  CHECK_EQ(layout.modifier(), gfx::NativePixmapHandle::kNoModifier);
  handle.modifier = layout.modifier();

  // Note: |buffer_usage| is not set. As a result, the constructed
  // NativePixmapFrameResource cannot be converted to a VideoFrame with the
  // method, CreateVideoFrame().
  return base::WrapRefCounted(new NativePixmapFrameResource(
      layout, visible_rect, natural_size, timestamp, *buffer_format,
      GetNextSharedMemoryId(), /*buffer_usage=*/std::nullopt,
      std::move(handle)));
}

scoped_refptr<NativePixmapFrameResource> NativePixmapFrameResource::Create(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage,
    scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap) {
  if (!pixmap) {
    return nullptr;
  }

  // This performs some validations and builds a VideoFrameLayout from |pixmap|
  // to be passed to the NativePixmapFrameResource constructor.
  if (!IsValidSize(pixmap->GetBufferSize(), visible_rect, natural_size)) {
    return nullptr;
  }

  const auto& buffer_format = pixmap->GetBufferFormat();
  auto pixel_format = GfxBufferFormatToVideoPixelFormat(buffer_format);
  if (!pixel_format) {
    DLOGF(ERROR) << " Unable to convert buffer format "
                 << gfx::BufferFormatToString(buffer_format)
                 << " to PixelFormat";
    return nullptr;
  }

  // Checks that the number of planes matches the expectation for the buffer
  // format.
  const size_t num_planes = pixmap->GetNumberOfPlanes();
  const size_t expected_number_of_planes =
      NumberOfPlanesForLinearBufferFormat(buffer_format);
  if (num_planes != expected_number_of_planes) {
    DLOGF(ERROR) << "Invalid number of planes=" << num_planes
                 << ", expected number of planes=" << expected_number_of_planes;
    return nullptr;
  }

  std::vector<media::ColorPlaneLayout> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = base::checked_cast<int32_t>(pixmap->GetDmaBufPitch(i));
    planes[i].offset = pixmap->GetDmaBufOffset(i);
    planes[i].size = pixmap->GetDmaBufPlaneSize(i);
  }

  auto layout = media::VideoFrameLayout::CreateWithPlanes(
      *pixel_format, pixmap->GetBufferSize(), std::move(planes),
      media::VideoFrameLayout::kBufferAddressAlignment,
      pixmap->GetBufferFormatModifier());
  if (!layout) {
    DLOGF(ERROR) << " Invalid layout";
    return nullptr;
  }

  return base::WrapRefCounted(new NativePixmapFrameResource(
      *layout, visible_rect, natural_size, timestamp, GetNextSharedMemoryId(),
      buffer_usage, std::move(pixmap)));
}

NativePixmapFrameResource::NativePixmapFrameResource(
    const media::VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferFormat buffer_format,
    gfx::GenericSharedMemoryId id,
    std::optional<gfx::BufferUsage> buffer_usage,
    gfx::NativePixmapHandle handle)
    : NativePixmapFrameResource(
          layout,
          visible_rect,
          natural_size,
          timestamp,
          id,
          buffer_usage,
          base::MakeRefCounted<gfx::NativePixmapDmaBuf>(layout.coded_size(),
                                                        buffer_format,
                                                        std::move(handle))) {}

NativePixmapFrameResource::NativePixmapFrameResource(
    const media::VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::GenericSharedMemoryId id,
    std::optional<gfx::BufferUsage> buffer_usage,
    scoped_refptr<const gfx::NativePixmapDmaBuf> pixmap)
    : pixmap_(std::move(pixmap)),
      id_(id),
      buffer_usage_(buffer_usage),
      layout_(layout),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      timestamp_(timestamp) {
  metadata().is_webgpu_compatible = pixmap_->SupportsZeroCopyWebGPUImport();
}

NativePixmapFrameResource::~NativePixmapFrameResource() {
  std::vector<base::OnceClosure> done_callbacks;
  {
    base::AutoLock lock(done_callbacks_lock_);
    done_callbacks = std::move(done_callbacks_);
  }
  for (auto& callback : done_callbacks) {
    std::move(callback).Run();
  }
}

const NativePixmapFrameResource*
NativePixmapFrameResource::AsNativePixmapFrameResource() const {
  return this;
}

bool NativePixmapFrameResource::IsMappable() const {
  return false;
}

const uint8_t* NativePixmapFrameResource::data(size_t plane) const {
  return nullptr;
}

uint8_t* NativePixmapFrameResource::writable_data(size_t plane) {
  return nullptr;
}

const uint8_t* NativePixmapFrameResource::visible_data(size_t plane) const {
  return nullptr;
}

uint8_t* NativePixmapFrameResource::GetWritableVisibleData(size_t plane) {
  return nullptr;
}

size_t NativePixmapFrameResource::NumDmabufFds() const {
  return pixmap_->GetNumberOfPlanes();
}

int NativePixmapFrameResource::GetDmabufFd(size_t i) const {
  return pixmap_->GetDmaBufFd(i);
}

scoped_refptr<const gfx::NativePixmapDmaBuf>
NativePixmapFrameResource::GetNativePixmapDmaBuf() const {
  return pixmap_;
}

gfx::GpuMemoryBufferHandle
NativePixmapFrameResource::CreateGpuMemoryBufferHandle() const {
  // Duplicate FD's into a new NativePixmapHandle
  gfx::NativePixmapHandle native_pixmap_handle = pixmap_->ExportHandle();
  if (native_pixmap_handle.planes.empty()) {
    return gfx::GpuMemoryBufferHandle();  // Invalid
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  // |gmb_handle.id| is set to the GenericSharedMemoryId from |this|. This
  // allows for more predictable caching when converting to a VideoFrame.
  gmb_handle.id = GetSharedMemoryId();
  gmb_handle.native_pixmap_handle = std::move(native_pixmap_handle);
  return gmb_handle;
}

std::unique_ptr<VideoFrame::ScopedMapping>
NativePixmapFrameResource::MapGMBOrSharedImage() const {
  // This accessor is used for frames with STORAGE_GPU_MEMORY_BUFFER. This class
  // is coded to advertise STORAGE_DMABUFS, so this always returns nullptr.
  return nullptr;
}

gfx::GenericSharedMemoryId NativePixmapFrameResource::GetSharedMemoryId()
    const {
  return id_;
}

const VideoFrameLayout& NativePixmapFrameResource::layout() const {
  return layout_;
}

VideoPixelFormat NativePixmapFrameResource::format() const {
  return layout_.format();
}

int NativePixmapFrameResource::stride(size_t plane) const {
  CHECK_LT(plane, layout().num_planes());
  return layout().planes()[plane].stride;
}

VideoFrame::StorageType NativePixmapFrameResource::storage_type() const {
  // TODO(nhebert): We should remove storage_type from FrameResource in favor of
  // HasDmabufs, HasGpuMemoryBuffer.
  return VideoFrame::STORAGE_DMABUFS;
}

int NativePixmapFrameResource::row_bytes(size_t plane) const {
  return VideoFrame::RowBytes(plane, format(), coded_size().width());
}

const gfx::Size& NativePixmapFrameResource::coded_size() const {
  return layout_.coded_size();
}

const gfx::Rect& NativePixmapFrameResource::visible_rect() const {
  return visible_rect_;
}

const gfx::Size& NativePixmapFrameResource::natural_size() const {
  return natural_size_;
}

gfx::ColorSpace NativePixmapFrameResource::ColorSpace() const {
  return color_space_;
}

void NativePixmapFrameResource::set_color_space(
    const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

const std::optional<gfx::HDRMetadata>& NativePixmapFrameResource::hdr_metadata()
    const {
  return hdr_metadata_;
}

void NativePixmapFrameResource::set_hdr_metadata(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  hdr_metadata_ = hdr_metadata;
}

const VideoFrameMetadata& NativePixmapFrameResource::metadata() const {
  return metadata_;
}

VideoFrameMetadata& NativePixmapFrameResource::metadata() {
  return metadata_;
}

void NativePixmapFrameResource::set_metadata(
    const VideoFrameMetadata& metadata) {
  metadata_ = metadata;
}

base::TimeDelta NativePixmapFrameResource::timestamp() const {
  return timestamp_;
}

void NativePixmapFrameResource::set_timestamp(base::TimeDelta timestamp) {
  timestamp_ = timestamp;
}

void NativePixmapFrameResource::AddDestructionObserver(
    base::OnceClosure callback) {
  CHECK(!callback.is_null());
  // TODO(nhebert): Add a UMA to see if this receives concurrent calls.
  base::AutoLock lock(done_callbacks_lock_);
  done_callbacks_.push_back(std::move(callback));
}

scoped_refptr<FrameResource> NativePixmapFrameResource::CreateWrappingFrame(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  if (!IsValidSize(coded_size(), visible_rect, natural_size)) {
    return nullptr;
  }

  // The wrapping frame simply copies all metadata from the original frame and
  // takes an additional reference to the NativePixmapDmaBuf. This is different
  // from VideoFrame's wrapping mechanism, which inserts a pointer to original
  // frame into the wrapping frame.
  // Note: Uses WrapRefCounted() since MakeRefCounted() cannot access a private
  // constructor.
  auto wrapping_frame = base::WrapRefCounted<NativePixmapFrameResource>(
      new NativePixmapFrameResource(layout(), visible_rect, natural_size,
                                    timestamp(), GetSharedMemoryId(),
                                    buffer_usage_, pixmap_));

  // All other metadata is copied to the "wrapping" frame.
  wrapping_frame->metadata().MergeMetadataFrom(metadata());
  wrapping_frame->set_color_space(ColorSpace());
  wrapping_frame->set_hdr_metadata(hdr_metadata());

  // Adds a reference to |this| from the wrapping frame via a destruction
  // observer. This avoids the original frame from returning to the frame pool
  // before the wrapping frame has been destroyed.
  wrapping_frame->AddDestructionObserver(base::DoNothingWithBoundArgs(
      base::WrapRefCounted<NativePixmapFrameResource>(this)));

  return wrapping_frame;
}

std::string NativePixmapFrameResource::AsHumanReadableString() const {
  if (metadata().end_of_stream) {
    return "end of stream";
  }

  std::ostringstream s;
  s << "format:" << format() << " coded_size:" << coded_size().ToString()
    << ", visible_rect:" << visible_rect_.ToString()
    << ", natural_size:" << natural_size_.ToString()
    << ", timestamp:" << timestamp_.InMicroseconds()
    << ", planes:" << pixmap_->GetNumberOfPlanes();
  return s.str();
}

gfx::GpuMemoryBufferHandle
NativePixmapFrameResource::GetGpuMemoryBufferHandleForTesting() const {
  // This accessor is used for frames with STORAGE_GPU_MEMORY_BUFFER. This class
  // is coded to advertise STORAGE_DMABUFS, so this always returns empty handle.
  return gfx::GpuMemoryBufferHandle();
}

scoped_refptr<VideoFrame> NativePixmapFrameResource::CreateVideoFrame() const {
  LOG_ASSERT(buffer_usage_.has_value())
      << "Unsupported conversion from wrapped DMA buffers to GpuMemoryBuffer "
         "VideoFrame.";

  // Creates a GMB-backed frame with using duplicated file descriptors.
  auto video_frame = CreateVideoFrameFromGpuMemoryBufferHandle(
      CreateGpuMemoryBufferHandle(), format(), coded_size(), visible_rect(),
      natural_size(), timestamp(), *buffer_usage_);
  if (!video_frame) {
    DLOGF(ERROR) << "Unable to create a VideoFrame";
    return nullptr;
  }

  // Copies VideoFrameMetadata from |this| to the output VideoFrame.
  video_frame->metadata().MergeMetadataFrom(metadata());
  video_frame->set_color_space(ColorSpace());
  video_frame->set_hdr_metadata(hdr_metadata());

  // Adds a reference to |this| from the output VideoFrame to make sure the
  // underlying frame does not get recycled back into the frame pool before it
  // is used.
  video_frame->AddDestructionObserver(base::DoNothingWithBoundArgs(
      base::WrapRefCounted<const NativePixmapFrameResource>(this)));

  return video_frame;
}

}  // namespace media
