// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_copy_strategy.h"

#include <dawn/native/DawnNative.h>

#include "base/bits.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_backing.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gpu {

namespace {

struct StagingBuffer {
  wgpu::Buffer buffer;
  gfx::Size plane_size;
  uint32_t bytes_per_row;
};

bool ComputeStagingBufferParams(const viz::SharedImageFormat& format,
                                const gfx::Size& size,
                                int plane_index,
                                uint32_t* bytes_per_row,
                                size_t* bytes_per_plane) {
  DCHECK(bytes_per_row);
  DCHECK(bytes_per_plane);

  std::optional<size_t> min_bytes_per_row(format.MaybeEstimatedPlaneSizeInBytes(
      plane_index, gfx::Size(size.width(), 1)));

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

  const gfx::Size plane_size = format.GetPlaneSize(plane_index, size);

  base::CheckedNumeric<size_t> aligned_bytes_per_plane = aligned_bytes_per_row;
  aligned_bytes_per_plane *= plane_size.height();

  return aligned_bytes_per_plane.AssignIfValid(bytes_per_plane);
}

// Allocate staging buffers. One staging buffer per plane.
bool AllocateStagingBuffers(wgpu::Device device,
                            const viz::SharedImageFormat& format,
                            const gfx::Size& size,
                            wgpu::BufferUsage usage,
                            bool map_at_creation,
                            std::vector<StagingBuffer>* buffers) {
  std::vector<StagingBuffer> staging_buffers;
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    uint32_t bytes_per_row;
    size_t bytes_per_plane;
    if (!ComputeStagingBufferParams(format, size, plane_index, &bytes_per_row,
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

    wgpu::Buffer buffer = device.CreateBuffer(&buffer_desc);

    const gfx::Size plane_size = format.GetPlaneSize(plane_index, size);

    staging_buffers.push_back({buffer, plane_size, bytes_per_row});
  }

  *buffers = std::move(staging_buffers);

  return true;
}

SkPixmap MappedStagingBufferToPixmap(const StagingBuffer& staging_buffer,
                                     const viz::SharedImageFormat& format,
                                     SkAlphaType alpha_type,
                                     const sk_sp<SkColorSpace>& color_space,
                                     int plane_index,
                                     bool writable) {
  const void* pixels_pointer =
      writable
          ? staging_buffer.buffer.GetMappedRange(0, wgpu::kWholeMapSize)
          : staging_buffer.buffer.GetConstMappedRange(0, wgpu::kWholeMapSize);

  DCHECK(pixels_pointer);

  auto info = SkImageInfo::Make(gfx::SizeToSkISize(staging_buffer.plane_size),
                                viz::ToClosestSkColorType(format, plane_index),
                                alpha_type, color_space);
  return SkPixmap(info, pixels_pointer, staging_buffer.bytes_per_row);
}

bool IsNonDawnGpuBacking(SharedImageBackingType type) {
  return type != SharedImageBackingType::kDawn &&
         type != SharedImageBackingType::kSharedMemory;
}

}  // namespace

DawnCopyStrategy::DawnCopyStrategy() = default;
DawnCopyStrategy::~DawnCopyStrategy() = default;

bool DawnCopyStrategy::CanCopy(SharedImageBacking* src_backing,
                               SharedImageBacking* dst_backing) {
  bool src_is_dawn = src_backing->GetType() == SharedImageBackingType::kDawn;
  bool dst_is_dawn = dst_backing->GetType() == SharedImageBackingType::kDawn;
  bool src_is_gpu = IsNonDawnGpuBacking(src_backing->GetType());
  bool dst_is_gpu = IsNonDawnGpuBacking(dst_backing->GetType());

  auto ret = (src_is_gpu && dst_is_dawn) || (src_is_dawn && dst_is_gpu);
  return ret;
}

bool DawnCopyStrategy::Copy(SharedImageBacking* src_backing,
                            SharedImageBacking* dst_backing) {
  if (IsNonDawnGpuBacking(src_backing->GetType()) &&
      dst_backing->GetType() == SharedImageBackingType::kDawn) {
    return CopyFromGpuBackingToDawn(
        src_backing, static_cast<DawnImageBacking*>(dst_backing));
  }

  CHECK(src_backing->GetType() == SharedImageBackingType::kDawn &&
        IsNonDawnGpuBacking(dst_backing->GetType()));
  return CopyFromDawnToGpuBacking(static_cast<DawnImageBacking*>(src_backing),
                                  dst_backing);
}

bool DawnCopyStrategy::CopyFromGpuBackingToDawn(SharedImageBacking* src,
                                                DawnImageBacking* dst) {
  wgpu::Device device = dst->device();
  wgpu::Texture texture = dst->GetTexture();
  if (!device || !texture) {
    LOG(ERROR) << "wgpu device or texture not present.";
    return false;
  }

  return CopyFromBackingToTexture(src, texture, device);
}

bool DawnCopyStrategy::CopyFromDawnToGpuBacking(DawnImageBacking* src,
                                                SharedImageBacking* dst) {
  wgpu::Device device = src->device();
  wgpu::Texture texture = src->GetTexture();
  if (!device || !texture) {
    LOG(ERROR) << "wgpu device or texture not present.";
    return false;
  }

  return CopyFromTextureToBacking(texture, dst, device);
}

// static
bool DawnCopyStrategy::CopyFromBackingToTexture(SharedImageBacking* src_backing,
                                                wgpu::Texture dst_texture,
                                                wgpu::Device device) {
  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
  internal_usage_desc.useInternalUsages = true;
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "DawnCopyStrategy::CopyFromBackingToTexture",
  };

  wgpu::CommandEncoder encoder =
      device.CreateCommandEncoder(&command_encoder_desc);

  const viz::SharedImageFormat format = src_backing->format();

  // Allocate staging buffers. One staging buffer per plane.
  std::vector<StagingBuffer> staging_buffers;
  if (!AllocateStagingBuffers(device, format, src_backing->size(),
                              wgpu::BufferUsage::CopySrc,
                              /*map_at_creation=*/true, &staging_buffers)) {
    LOG(ERROR) << "Failed to allocate staging buffers.";
    return false;
  }

  CHECK_EQ(static_cast<size_t>(format.NumberOfPlanes()),
           staging_buffers.size());

  std::vector<SkPixmap> staging_pixmaps;
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    staging_pixmaps.push_back(MappedStagingBufferToPixmap(
        staging_buffers[plane_index], format, src_backing->alpha_type(),
        src_backing->color_space().ToSkColorSpace(), plane_index,
        /*writable=*/true));
  }

  // Read data from backing to the staging buffers
  if (!src_backing->ReadbackToMemory(staging_pixmaps)) {
    LOG(ERROR) << "Failed to read back from backing to memory.";
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

    wgpu::TexelCopyBufferInfo buffer_copy = {
        .layout =
            {
                .bytesPerRow = bytes_per_row,
                .rowsPerImage = wgpu::kCopyStrideUndefined,
            },
        .buffer = buffer.Get(),
    };
    wgpu::TextureFormat wgpu_format = dst_texture.GetFormat();
    bool is_yuv_plane =
        (wgpu_format == wgpu::TextureFormat::R8BG8Biplanar420Unorm ||
         wgpu_format == wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm ||
         wgpu_format == wgpu::TextureFormat::R8BG8A8Triplanar420Unorm);
    // Get proper plane aspect for multiplanar textures.
    wgpu::TexelCopyTextureInfo texture_copy = {
        .texture = dst_texture,
        .aspect = ToDawnTextureAspect(is_yuv_plane, plane_index),
    };
    wgpu::Extent3D extent = {static_cast<uint32_t>(plane_size.width()),
                             static_cast<uint32_t>(plane_size.height()), 1};
    encoder.CopyBufferToTexture(&buffer_copy, &texture_copy, &extent);
  }

  wgpu::CommandBuffer commandBuffer = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commandBuffer);

  return true;
}

// static
bool DawnCopyStrategy::CopyFromTextureToBacking(wgpu::Texture src_texture,
                                                SharedImageBacking* dst_backing,
                                                wgpu::Device device) {
  wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
  internal_usage_desc.useInternalUsages = true;
  wgpu::CommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc,
      .label = "DawnCopyStrategy::CopyFromTextureToBacking",
  };

  wgpu::CommandEncoder encoder =
      device.CreateCommandEncoder(&command_encoder_desc);

  const viz::SharedImageFormat format = dst_backing->format();

  // Allocate staging buffers. One staging buffer per plane.
  std::vector<StagingBuffer> staging_buffers;
  if (!AllocateStagingBuffers(
          device, format, dst_backing->size(),
          wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
          /*map_at_creation=*/false, &staging_buffers)) {
    LOG(ERROR) << "Failed to allocate staging buffers.";
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

    wgpu::TextureFormat wgpu_format = src_texture.GetFormat();
    bool is_yuv_plane =
        (wgpu_format == wgpu::TextureFormat::R8BG8Biplanar420Unorm ||
         wgpu_format == wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm ||
         wgpu_format == wgpu::TextureFormat::R8BG8A8Triplanar420Unorm);
    // Get proper plane aspect for multiplanar textures.
    wgpu::TexelCopyTextureInfo texture_copy = {
        .texture = src_texture,
        .aspect = ToDawnTextureAspect(is_yuv_plane, plane_index),
    };
    wgpu::TexelCopyBufferInfo buffer_copy = {
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

  wgpu::Queue queue = device.GetQueue();
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
        [](wgpu::MapAsyncStatus status, wgpu::StringView, bool* success) {
          *success = status == wgpu::MapAsyncStatus::Success;
        },
        &success)};

    if (device.GetAdapter().GetInstance().WaitAny(1, &wait_info, UINT64_MAX) !=
        wgpu::WaitStatus::Success) {
      LOG(ERROR) << "WaitAny failed while mapping staging buffer for read.";
      return false;
    }

    if (!wait_info.completed || !success) {
      LOG(ERROR) << "MapAsync did not yield a readable mapping.";
      return false;
    }

    staging_pixmaps.push_back(MappedStagingBufferToPixmap(
        staging_buffers[plane_index], format, dst_backing->alpha_type(),
        dst_backing->color_space().ToSkColorSpace(), plane_index,
        /*writable=*/false));
  }

  return dst_backing->UploadFromMemory(staging_pixmaps);
}

}  // namespace gpu
