// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"

#include <dawn/native/DawnNative.h>

#include "base/bits.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gpu {

DawnFallbackImageRepresentation::DawnFallbackImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::TextureFormat wgpu_format,
    std::vector<wgpu::TextureFormat> view_formats)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      wgpu_format_(wgpu_format),
      view_formats_(std::move(view_formats)) {}

DawnFallbackImageRepresentation::~DawnFallbackImageRepresentation() = default;

bool DawnFallbackImageRepresentation::ComputeStagingBufferParams(
    int plane_index,
    uint32_t* bytes_per_row,
    size_t* bytes_per_plane) const {
  DCHECK(bytes_per_row);
  DCHECK(bytes_per_plane);

  const viz::SharedImageFormat format = this->format();

  std::optional<size_t> min_bytes_per_row(format.MaybeEstimatedPlaneSizeInBytes(
      plane_index, gfx::Size(size().width(), 1)));

  if (!min_bytes_per_row.has_value()) {
    return false;
  }

  // Align up to 256, required by WebGPU buffer->texture and texture->buffer
  // copies.
  base::CheckedNumeric<uint32_t> aligned_bytes_per_row =
      base::bits::AlignUp(*min_bytes_per_row, size_t{256});
  if (!aligned_bytes_per_row.AssignIfValid(bytes_per_row)) {
    return false;
  }
  if (*bytes_per_row < *min_bytes_per_row) {
    // Overflow in AlignUp.
    return false;
  }

  const gfx::Size plane_size = format.GetPlaneSize(plane_index, size());

  base::CheckedNumeric<size_t> aligned_bytes_per_plane = aligned_bytes_per_row;
  aligned_bytes_per_plane *= plane_size.height();

  return aligned_bytes_per_plane.AssignIfValid(bytes_per_plane);
}

// Allocate staging buffers. One staging buffer per plane.
bool DawnFallbackImageRepresentation::AllocateStagingBuffers(
    wgpu::BufferUsage usage,
    bool map_at_creation,
    std::vector<StagingBuffer>* buffers) {
  std::vector<StagingBuffer> staging_buffers;
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       ++plane_index) {
    uint32_t bytes_per_row;
    size_t bytes_per_plane;
    if (!ComputeStagingBufferParams(plane_index, &bytes_per_row,
                                    &bytes_per_plane)) {
      return false;
    }

    // Create a staging buffer to hold pixel data which will be uploaded into
    // a texture.
    wgpu::BufferDescriptor buffer_desc = {
        .usage = usage,
        .size = bytes_per_plane,
        .mappedAtCreation = map_at_creation,
    };

    wgpu::Buffer buffer = device_.CreateBuffer(&buffer_desc);

    const gfx::Size plane_size = format().GetPlaneSize(plane_index, size());

    staging_buffers.push_back({buffer, plane_size, bytes_per_row});
  }

  *buffers = std::move(staging_buffers);

  return true;
}

SkPixmap DawnFallbackImageRepresentation::MappedStagingBufferToPixmap(
    const StagingBuffer& staging_buffer,
    int plane_index,
    bool writable) {
  const void* pixels_pointer =
      writable
          ? staging_buffer.buffer.GetMappedRange(0, wgpu::kWholeMapSize)
          : staging_buffer.buffer.GetConstMappedRange(0, wgpu::kWholeMapSize);

  DCHECK(pixels_pointer);

  auto info =
      SkImageInfo::Make(gfx::SizeToSkISize(staging_buffer.plane_size),
                        viz::ToClosestSkColorType(
                            /*gpu_compositing=*/true, format(), plane_index),
                        alpha_type(), color_space().ToSkColorSpace());
  return SkPixmap(info, pixels_pointer, staging_buffer.bytes_per_row);
}

bool DawnFallbackImageRepresentation::ReadbackFromBacking() {
  // Copy from the staging WGPUBuffer into the wgpu::Texture.
  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
  internal_usage_desc.useInternalUsages = true;
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
  };

  wgpu::CommandEncoder encoder =
      device_.CreateCommandEncoder(&command_encoder_desc);

  const viz::SharedImageFormat format = this->format();

  // Allocate staging buffers. One staging buffer per plane.
  std::vector<StagingBuffer> staging_buffers;
  if (!AllocateStagingBuffers(wgpu::BufferUsage::CopySrc,
                              /*map_at_creation=*/true, &staging_buffers)) {
    return false;
  }

  CHECK_EQ(static_cast<size_t>(format.NumberOfPlanes()),
           staging_buffers.size());

  std::vector<SkPixmap> staging_pixmaps;
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    staging_pixmaps.push_back(MappedStagingBufferToPixmap(
        staging_buffers[plane_index], plane_index, /*writable=*/true));
  }

  // Read data from backing to the staging buffers
  if (!backing()->ReadbackToMemory(staging_pixmaps)) {
    return false;
  }

  // Copy the staging buffers to texture.
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    const auto& staging_buffer_entry = staging_buffers[plane_index];
    wgpu::Buffer buffer = staging_buffer_entry.buffer;
    uint32_t bytes_per_row = staging_buffer_entry.bytes_per_row;
    const auto& plane_size = staging_buffer_entry.plane_size;

    // Unmap the buffer.
    buffer.Unmap();

    wgpu::ImageCopyBuffer buffer_copy = {
        .layout =
            {
                .bytesPerRow = bytes_per_row,
                .rowsPerImage = wgpu::kCopyStrideUndefined,
            },
        .buffer = buffer.Get(),
    };
    bool is_yuv_plane =
        (wgpu_format_ == wgpu::TextureFormat::R8BG8Biplanar420Unorm ||
         wgpu_format_ == wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm ||
         wgpu_format_ == wgpu::TextureFormat::R8BG8A8Triplanar420Unorm);
    // Get proper plane aspect for multiplanar textures.
    wgpu::ImageCopyTexture texture_copy = {
        .texture = texture_,
        .aspect = ToDawnTextureAspect(is_yuv_plane, plane_index),
    };
    wgpu::Extent3D extent = {static_cast<uint32_t>(plane_size.width()),
                             static_cast<uint32_t>(plane_size.height()), 1};
    encoder.CopyBufferToTexture(&buffer_copy, &texture_copy, &extent);
  }

  wgpu::CommandBuffer commandBuffer = encoder.Finish();

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commandBuffer);

  return true;
}

bool DawnFallbackImageRepresentation::UploadToBacking() {
  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
  internal_usage_desc.useInternalUsages = true;
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
  };

  wgpu::CommandEncoder encoder =
      device_.CreateCommandEncoder(&command_encoder_desc);

  const viz::SharedImageFormat format = this->format();

  // Allocate staging buffers. One staging buffer per plane.
  std::vector<StagingBuffer> staging_buffers;
  if (!AllocateStagingBuffers(
          wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
          /*map_at_creation=*/false, &staging_buffers)) {
    return false;
  }

  CHECK_EQ(static_cast<size_t>(format.NumberOfPlanes()),
           staging_buffers.size());

  // Copy from texture to staging buffers.
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    const auto& staging_buffer_entry = staging_buffers[plane_index];
    wgpu::Buffer buffer = staging_buffer_entry.buffer;
    uint32_t bytes_per_row = staging_buffer_entry.bytes_per_row;
    const auto& plane_size = staging_buffer_entry.plane_size;

    bool is_yuv_plane =
        (wgpu_format_ == wgpu::TextureFormat::R8BG8Biplanar420Unorm ||
         wgpu_format_ == wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm ||
         wgpu_format_ == wgpu::TextureFormat::R8BG8A8Triplanar420Unorm);
    // Get proper plane aspect for multiplanar textures.
    wgpu::ImageCopyTexture texture_copy = {
        .texture = texture_,
        .aspect = ToDawnTextureAspect(is_yuv_plane, plane_index),
    };
    wgpu::ImageCopyBuffer buffer_copy = {
        .layout =
            {
                .bytesPerRow = bytes_per_row,
                .rowsPerImage = wgpu::kCopyStrideUndefined,
            },
        .buffer = buffer,
    };
    wgpu::Extent3D extent = {static_cast<uint32_t>(plane_size.width()),
                             static_cast<uint32_t>(plane_size.height()), 1};

    encoder.CopyTextureToBuffer(&texture_copy, &buffer_copy, &extent);
  }

  wgpu::CommandBuffer commandBuffer = encoder.Finish();

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commandBuffer);

  // Map the staging buffer for read.
  std::vector<SkPixmap> staging_pixmaps;
  for (int plane_index = 0;
       plane_index < static_cast<int>(staging_buffers.size()); ++plane_index) {
    const auto& staging_buffer_entry = staging_buffers[plane_index];

    bool success = false;
    wgpu::FutureWaitInfo wait_info = {staging_buffer_entry.buffer.MapAsync(
        wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::MapAsyncStatus status, const char*, bool* success) {
          *success = status == wgpu::MapAsyncStatus::Success;
        },
        &success)};

    if (device_.GetAdapter().GetInstance().WaitAny(1, &wait_info, UINT64_MAX) !=
        wgpu::WaitStatus::Success) {
      return false;
    }

    if (!wait_info.completed || !success) {
      return false;
    }

    staging_pixmaps.push_back(MappedStagingBufferToPixmap(
        staging_buffers[plane_index], plane_index, /*writable=*/false));
  }

  return backing()->UploadFromMemory(staging_pixmaps);
}

wgpu::Texture DawnFallbackImageRepresentation::BeginAccess(
    wgpu::TextureUsage wgpu_texture_usage,
    wgpu::TextureUsage internal_usage) {
  const std::string debug_label = "DawnFallbackSharedImageRep(" +
                                  CreateLabelForSharedImageUsage(usage()) + ")";

  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.label = debug_label.c_str();
  texture_descriptor.format = wgpu_format_;
  texture_descriptor.usage = wgpu_texture_usage;

  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount = view_formats_.size();
  texture_descriptor.viewFormats = view_formats_.data();

  // Note: The texture must be internally copyable as this class itself uses the
  // texture as the dest and source of copies for readback from and upload to
  // the backing respectively.
  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = internal_usage | wgpu::TextureUsage::CopySrc |
                               wgpu::TextureUsage::CopyDst;

  texture_descriptor.nextInChain = &internalDesc;

  texture_ = device_.CreateTexture(&texture_descriptor);

  // Copy data from the image's backing to the texture. We only do it if the
  // image is marked as cleared/initialized.
  if (IsCleared() && !ReadbackFromBacking()) {
    texture_ = nullptr;
  }

  return texture_;
}

void DawnFallbackImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // Upload the texture's content to the backing. Only do it if the texture is
  // initialized.
  if (dawn::native::IsTextureSubresourceInitialized(
          texture_.Get(), /*baseMipLevel=*/0, /*levelCount=*/1,
          /*baseArrayLayer=*/0,
          /*layerCount=*/1) &&
      UploadToBacking()) {
    SetCleared();
  }

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  texture_.Destroy();

  texture_ = nullptr;
}

}  // namespace gpu
