// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/d3d11_shared_image_buffer_tracker.h"

#include <optional>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "gpu/ipc/common/exported_shared_image.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/win/d3d11_utils.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/buffer_types.h"

namespace media {

namespace {

// WebGPU read usage is conditional: as in DefaultTexture2DWrapper (see
// media/gpu/windows/d3d11_texture_wrapper.cc), sharing NV12 textures from
// D3D11 to D3D12 may show corruption on some Intel GPUs, so the usage is only
// added when the corresponding workaround is absent.
gpu::SharedImageUsageSet GetSharedImageUsage(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_VIDEO_ENCODE_ACCELERATOR;
  if (!workarounds.disable_sharing_nv12_from_d3d11_to_d3d12) {
    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }
  return usage;
}

// Slot shared images must be created with a valid color space; normalize an
// invalid one the same way the GMB consumer path does.
gfx::ColorSpace SanitizeColorSpace(const gfx::ColorSpace& color_space) {
  return color_space.IsValid() ? color_space : gfx::ColorSpace::CreateREC601();
}

}  // namespace

D3D11SharedImageBufferTracker::D3D11SharedImageBufferTracker(
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)),
      d3d_device_(dxgi_device_manager_->GetDevice()) {}

D3D11SharedImageBufferTracker::~D3D11SharedImageBufferTracker() {
  VideoCaptureGpuChannelHost::GetInstance().RemoveObserver(this);
}

bool D3D11SharedImageBufferTracker::Init(const gfx::Size& dimensions,
                                         VideoPixelFormat format,
                                         const mojom::PlaneStridesPtr& strides,
                                         const gfx::ColorSpace& color_space) {
  if (format != PIXEL_FORMAT_NV12) {
    DLOG(ERROR) << "Unsupported VideoPixelFormat "
                << VideoPixelFormatToString(format);
    return false;
  }

  if (!d3d_device_) {
    LOG(ERROR) << "No D3D11 device available to back a capture slot.";
    return false;
  }

  const std::optional<size_t> premapped_size =
      viz::SharedMemorySizeForSharedImageFormat(viz::MultiPlaneFormat::kNV12,
                                                dimensions);
  const std::optional<size_t> premapped_stride =
      viz::SharedMemoryRowSizeForSharedImageFormat(
          viz::MultiPlaneFormat::kNV12, /*plane_index=*/0, dimensions.width());
  if (!premapped_size || !premapped_stride) {
    DLOG(ERROR) << "Invalid NV12 capture slot dimensions "
                << dimensions.ToString();
    return false;
  }

  auto sii =
      VideoCaptureGpuChannelHost::GetInstance().GetSharedImageInterface();
  if (!sii || sii->IsLost()) {
    LOG(ERROR) << "SharedImageInterface unavailable or lost.";
    return false;
  }

  // Allocate the premapped region upfront; it is populated on demand in
  // DuplicateAsUnsafeRegion() and travels with the shared image through the
  // DXGI handle attached below, so software consumers can read the frame
  // without a GPU readback (mirrors GpuMemoryBufferTrackerWin). It is
  // allocated before the texture so that |dxgi_handle_| can be assigned once,
  // with its region already attached.
  premapped_region_ = base::UnsafeSharedMemoryRegion::Create(*premapped_size);
  premapped_mapping_ = premapped_region_.Map();
  if (!premapped_region_.IsValid() || !premapped_mapping_.IsValid()) {
    LOG(ERROR) << "Failed to allocate the premapped region for a capture slot.";
    ResetSlot();
    return false;
  }

  base::win::ScopedHandle scoped_handle = CreateNV12Texture(
      d3d_device_.Get(), dimensions, "Camera_SharedImageBufferTracker");
  if (!scoped_handle.is_valid()) {
    ResetSlot();
    return false;
  }

  const gfx::DXGIHandle texture_handle(std::move(scoped_handle));
  dxgi_handle_ = texture_handle.CloneWithRegion(premapped_region_.Duplicate());
  if (!dxgi_handle_.IsValid() || !dxgi_handle_.region().IsValid()) {
    LOG(ERROR) << "Failed to duplicate the capture slot handles.";
    ResetSlot();
    return false;
  }

  // Import the slot texture into a shared image. The handle passed in is
  // consumed by the GPU process, so pass a duplicate and keep |dxgi_handle_|
  // for the producer's copy. The color space is fixed at creation: frames
  // delivered from this slot must carry the same color space, which
  // VideoFrame::set_color_space() enforces for shared-image-backed frames.
  // Import through the buffer-usage overload (like the GMB capture path) so
  // the client-side shared image keeps a MappableBuffer around the handle;
  // Export(with_buffer_handle=true) in GetVideoBufferHandle() then carries the
  // premapped region attached to |dxgi_handle_| to consumers.
  const gfx::ColorSpace sanitized_color_space = SanitizeColorSpace(color_space);
  auto channel = VideoCaptureGpuChannelHost::GetInstance().GetGpuChannel();
  const gpu::GpuDriverBugWorkarounds workarounds =
      channel
          ? gpu::GpuDriverBugWorkarounds(
                channel->gpu_feature_info().enabled_gpu_driver_bug_workarounds)
          : gpu::GpuDriverBugWorkarounds();
  shared_image_ = sii->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, dimensions, sanitized_color_space,
       GetSharedImageUsage(workarounds), "MediaFoundationVideoCaptureSlot"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::GpuMemoryBufferHandle(dxgi_handle_.Clone()));
  if (!shared_image_) {
    LOG(ERROR) << "Failed to create shared image from capture slot texture.";
    ResetSlot();
    return false;
  }

  // The premapped region only reaches consumers if the shared image kept a
  // client-side mappable buffer around the handle: Export() attaches the
  // buffer handle only when one exists, and the renderer then calls
  // ClientSharedImage::SetUsePreMappedMemory() - which CHECKs on that buffer -
  // for every Windows shared-image frame. Reject the slot here so the failure
  // surfaces as a capture-side allocation error instead of a consumer crash.
  if (shared_image_->GetGpuMemoryBufferType() != gfx::DXGI_SHARED_HANDLE) {
    LOG(ERROR) << "Capture slot shared image is not mappable.";
    ResetSlot();
    return false;
  }

  // Verify the creation sync token on the capture sequence so that downstream
  // Export() calls (e.g. in GetVideoBufferHandle()) do not trigger a
  // synchronous IPC. Note that Export() also drains the image's pending
  // sync-token map into the discarded result, so nothing may queue GPU work on
  // |shared_image_| between its creation and this call.
  shared_image_->Export();

  dimensions_ = dimensions;
  color_space_ = sanitized_color_space;
  stride_ = base::checked_cast<uint32_t>(*premapped_stride);
  VideoCaptureGpuChannelHost::GetInstance().AddObserver(this);
  is_valid_ = true;
  return true;
}

void D3D11SharedImageBufferTracker::ResetSlot() {
  is_valid_ = false;
  shared_image_.reset();
  dxgi_handle_ = gfx::DXGIHandle();
  premapped_mapping_ = base::WritableSharedMemoryMapping();
  premapped_region_ = base::UnsafeSharedMemoryRegion();
  staging_texture_.Reset();
}

bool D3D11SharedImageBufferTracker::IsD3DDeviceChanged() {
  if (!HasD3DDeviceChanged(dxgi_device_manager_.get(), d3d_device_.Get())) {
    return false;
  }
  // The slot cannot be recreated here, so drop it: marking it invalid both
  // stops the pool from reusing it and stops re-probing D3D for it on every
  // reservation, since the pool walks every tracker looking for a match.
  is_valid_ = false;
  return true;
}

bool D3D11SharedImageBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides,
    const gfx::ColorSpace& color_space) {
  return is_valid_ && !IsD3DDeviceChanged() && (format == PIXEL_FORMAT_NV12) &&
         (dimensions == dimensions_) &&
         (SanitizeColorSpace(color_space) == color_space_);
}

uint32_t D3D11SharedImageBufferTracker::GetMemorySizeInBytes() {
  base::CheckedNumeric<size_t> size = 0;
  if (shared_image_) {
    size += shared_image_->EstimatedSizeInBytes().InBytes();
  }
  if (premapped_region_.IsValid()) {
    size += premapped_region_.GetSize();
  }
  return base::checked_cast<uint32_t>(size.ValueOrDie());
}

std::unique_ptr<VideoCaptureBufferHandle>
D3D11SharedImageBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
}

base::UnsafeSharedMemoryRegion
D3D11SharedImageBufferTracker::DuplicateAsUnsafeRegion() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "D3D11SharedImageBufferTracker::DuplicateAsUnsafeRegion");

  if (!dxgi_handle_.IsValid()) {
    return base::UnsafeSharedMemoryRegion();
  }

  CHECK(premapped_region_.IsValid());
  CHECK(premapped_mapping_.IsValid());

  if (!gpu::CopyDXGIBufferToShMem(dxgi_handle_.buffer_handle(),
                                  premapped_mapping_.GetMemoryAsSpan<uint8_t>(),
                                  d3d_device_.Get(), &staging_texture_)) {
    DLOG(ERROR) << "Couldn't copy DXGI buffer to shmem";
    return base::UnsafeSharedMemoryRegion();
  }

  return premapped_region_.Duplicate();
}

gfx::GpuMemoryBufferHandle
D3D11SharedImageBufferTracker::GetGpuMemoryBufferHandle() {
  if (IsD3DDeviceChanged()) {
    return gfx::GpuMemoryBufferHandle();
  }
  // Clone() carries the premapped region attached in Init() along with the
  // duplicated texture handle.
  gfx::GpuMemoryBufferHandle handle(dxgi_handle_.Clone());
  handle.offset = 0;
  handle.stride = stride_;
  return handle;
}

media::mojom::VideoBufferHandlePtr
D3D11SharedImageBufferTracker::GetVideoBufferHandle() {
  if (!shared_image_) {
    return nullptr;
  }
  // Export with the buffer handle so the premapped region attached to the
  // DXGI handle travels with the shared image; the renderer serves software
  // consumers from it through MappableBufferDXGI without a GPU readback.
  auto exported_shared_image =
      shared_image_->Export(/*with_buffer_handle=*/true);
  auto handle_set = media::mojom::SharedImageBufferHandleSet::New(
      std::move(exported_shared_image), shared_image_->creation_sync_token(),
      // Capture slots are reused and rewritten in place between frames, so
      // consumers must flag the backing as externally updated before each
      // read.
      /*is_rewritten_in_place=*/true);
  return media::mojom::VideoBufferHandle::NewSharedImageHandle(
      std::move(handle_set));
}

VideoCaptureBufferType D3D11SharedImageBufferTracker::GetBufferType() {
  return VideoCaptureBufferType::kSharedImage;
}

void D3D11SharedImageBufferTracker::OnContextLost() {
  is_valid_ = false;
}

}  // namespace media
