// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_

#include <stdint.h>

#include <initializer_list>
#include <string>

#include "gpu/gpu_export.h"

namespace gpu {

// Please update the function, CreateLabelForSharedImageUsage, when adding a new
// enum value.
enum SharedImageUsage : uint32_t {
  // Image will be read via GLES2Interface
  SHARED_IMAGE_USAGE_GLES2_READ = 1 << 0,
  // Image will be read via RasterInterface
  SHARED_IMAGE_USAGE_RASTER_READ = 1 << 1,
  // Image will be read from inside Display Compositor
  SHARED_IMAGE_USAGE_DISPLAY_READ = 1 << 2,
  // Image will be written to inside Display Compositor
  SHARED_IMAGE_USAGE_DISPLAY_WRITE = 1 << 3,
  // Image will be used as a scanout buffer (overlay)
  SHARED_IMAGE_USAGE_SCANOUT = 1 << 4,
  // Image will be used in OOP rasterization. This flag is used on top of
  // SHARED_IMAGE_USAGE_RASTER_{READ, WRITE} to indicate that the client will
  // only use RasterInterface for OOP rasterization. TODO(backer): Eliminate
  // once we can CPU raster to SkImage via RasterInterface.
  SHARED_IMAGE_USAGE_OOP_RASTERIZATION = 1 << 5,
  // Image will be read by Dawn (for WebGPU)
  SHARED_IMAGE_USAGE_WEBGPU_READ = 1 << 6,
  // Image may use concurrent read/write access. Used by single buffered canvas.
  // TODO(crbug.com/41462072): This usage is currently not supported in
  // GL/Vulkan
  // interop cases.
  SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE = 1 << 7,
  // Image will be used for video decode acceleration on Chrome OS.
  SHARED_IMAGE_USAGE_VIDEO_DECODE = 1 << 8,
  // Image will be used as a WebGPU swapbuffer
  SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE = 1 << 9,
  // The image was created by VideoToolbox on macOS, and is backed by a
  // CVPixelBuffer's IOSurface. Because of this backing, IOSurfaceIsInUse will
  // always return true.
  SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX = 1 << 10,
  // Image will be used with mipmap enabled
  SHARED_IMAGE_USAGE_MIPMAP = 1 << 11,
  // Image will be used for CPU Writes by client
  SHARED_IMAGE_USAGE_CPU_WRITE = 1 << 12,
  // Image will be used in RasterInterface with RawDraw.
  SHARED_IMAGE_USAGE_RAW_DRAW = 1 << 13,
  // Image will be used in RasterInterface for DelegatedCompositing.
  // This is intended to avoid the overhead of a GPU fence per tile.
  // TODO(crbug.com/41492887): In order to delegate buffers we need all buffer
  // allocations to be set as SCANOUT. This will cause a fence per rastered
  // tiled. A new buffer concept that avoids scanout but allows delegation might
  // enable us to remove this usage.
  SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING = 1 << 14,
  // Image will be created on the high performance GPU if supported.
  SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU = 1 << 15,
  // Windows only: image will be backed by a DComp surface. A swap chain is
  // preferred when an image is opaque and expected to update frequently and
  // independently of other overlays. This flag is incompatible with
  // DISPLAY_READ and SCANOUT_DXGI_SWAP_CHAIN.
  SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE = 1 << 16,
  // Windows only: image will be backed by a DXGI swap chain. This flag is
  // incompatible with SCANOUT_DCOMP_SURFACE.
  SHARED_IMAGE_USAGE_SCANOUT_DXGI_SWAP_CHAIN = 1 << 17,

  // Image will be used as a WebGPU storage texture.
  SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE = 1 << 18,

  // Image will be written via GLES2Interface
  SHARED_IMAGE_USAGE_GLES2_WRITE = 1 << 19,

  // Image will be written via RasterInterface
  SHARED_IMAGE_USAGE_RASTER_WRITE = 1 << 20,

  // Image will be written by Dawn (for WebGPU)
  SHARED_IMAGE_USAGE_WEBGPU_WRITE = 1 << 21,

  // The image will be used by GLES2 only for raster over the GLES2 interface.
  // Specified in conjunction with GLES2_READ and/or GLES2_WRITE.
  SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY = 1 << 22,

  // The image will be used by raster only over the GLES2 interface.
  // Specified in conjunction with RASTER_READ and/or RASTER_WRITE.
  SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY = 1 << 23,

  // Image will contain protected content to be scanned out. Note that this type
  // of image
  // won't necessarily be written to by a hardware video decoder, but will
  // instead be written
  // to by a preprocessing step that converts the image's pixel format into
  // something the
  // display controller understands.
  SHARED_IMAGE_USAGE_PROTECTED_VIDEO = 1 << 24,

  // Image will be used as a WebGPU shared buffer
  SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER = 1 << 25,

  // Start service side only usage flags after this entry. They must be larger
  // than `LAST_CLIENT_USAGE`.
  LAST_CLIENT_USAGE = SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER,

  // Image will have pixels uploaded from CPU. The backing must implement
  // `UploadFromMemory()` if it supports this usage. Clients should specify
  // SHARED_IMAGE_USAGE_CPU_WRITE if they need to write pixels to the image.
  SHARED_IMAGE_USAGE_CPU_UPLOAD = 1 << 26,

  LAST_SHARED_IMAGE_USAGE = SHARED_IMAGE_USAGE_CPU_UPLOAD
};

class GPU_EXPORT SharedImageUsageSet {
 public:
  constexpr SharedImageUsageSet() = default;
  // Permanent nolint to allow for natural conversion from mask to set.
  // NOLINTBEGIN(google-explicit-constructor)
  constexpr SharedImageUsageSet(SharedImageUsage mask) : set_storage_(mask) {}
  // NOLINTEND(google-explicit-constructor)

  // TODO(crbug.com/343347288): Eventually we should deprecate this constructor
  // and replace its usage with a function named something like
  // 'UntypedMaskCast'. This will allow easly track any remaining non typed
  // usage.
  explicit constexpr SharedImageUsageSet(uint32_t mask) : set_storage_(mask) {}

  constexpr SharedImageUsageSet(
      std::initializer_list<SharedImageUsage> usages) {
    for (auto usage : usages) {
      set_storage_ |= usage;
    }
  }

  // Unions with 'set_b' and stores result in self.
  inline constexpr void PutAll(gpu::SharedImageUsageSet set_b) {
    set_storage_ = set_storage_ | static_cast<uint32_t>(set_b);
  }

  // Removes all elements of input set from this set.
  inline constexpr void RemoveAll(gpu::SharedImageUsageSet set_b) {
    uint32_t negation_mask = ~set_b.set_storage_;
    set_storage_ &= negation_mask;
  }

  // Returns true iff our set is empty.
  constexpr bool empty() const { return set_storage_ == 0; }

  // The semantic expectation here is that 'Has' is for set testing of single
  // elements.
  inline constexpr bool Has(gpu::SharedImageUsage set_b) const {
    return (set_storage_ & set_b) == set_b;
  }

  // These function are intentionally deleted. Use the 'Has' function as
  // 'SharedImageUsage' is conceptually not a set.
  inline constexpr bool HasAll(gpu::SharedImageUsage set_b) const = delete;
  inline constexpr bool HasAny(gpu::SharedImageUsage set_b) const = delete;

  // Test set membership via intersection. Returns true if 'set_b' is a subset.
  inline constexpr bool HasAll(gpu::SharedImageUsageSet set_b) const {
    return (set_storage_ & set_b.set_storage_) == set_b.set_storage_;
  }

  // Test set membership via intersection.
  inline constexpr bool HasAny(gpu::SharedImageUsageSet set_b) const {
    return (set_storage_ & set_b.set_storage_) != 0;
  }

  inline constexpr void operator|=(gpu::SharedImageUsageSet mask_b) {
    PutAll(mask_b);
  }

  // Temporary exception to allow for existing, non type safe, conversions.
  // TODO(crbug.com/343347288): Remove after all usage has been converted to
  // `SharedImageUsageSet`.
  // NOLINTBEGIN(google-explicit-constructor)
  inline constexpr operator uint32_t() const { return set_storage_; }
  // NOLINTEND(google-explicit-constructor)

 private:
  friend inline constexpr bool operator==(gpu::SharedImageUsageSet set_a,
                                          gpu::SharedImageUsageSet set_b);

  friend inline constexpr gpu::SharedImageUsageSet operator|(
      gpu::SharedImageUsageSet set_a,
      gpu::SharedImageUsage mask_b);
  friend inline constexpr gpu::SharedImageUsageSet operator|(
      gpu::SharedImageUsage mask_a,
      gpu::SharedImageUsageSet set_b);
  friend inline constexpr gpu::SharedImageUsageSet operator|(
      gpu::SharedImageUsage mask_a,
      gpu::SharedImageUsage mask_b);

  friend inline constexpr const SharedImageUsageSet Intersection(
      gpu::SharedImageUsageSet set_a,
      gpu::SharedImageUsageSet set_b);
  uint32_t set_storage_ = 0;
};

inline constexpr const SharedImageUsageSet Intersection(
    gpu::SharedImageUsageSet set_a,
    gpu::SharedImageUsageSet set_b) {
  return SharedImageUsageSet(set_a.set_storage_ & set_b.set_storage_);
}
// The global operators below cause 'SharedImageUsage' operations to result in
// 'SharedImageUsageSet' and avoid the ambiguity with uint32_t.
inline constexpr gpu::SharedImageUsageSet operator|(
    gpu::SharedImageUsageSet set_a,
    gpu::SharedImageUsageSet set_b) {
  set_a.PutAll(set_b);
  return set_a;
}

inline constexpr gpu::SharedImageUsageSet operator|(
    gpu::SharedImageUsageSet set_a,
    gpu::SharedImageUsage mask_b) {
  set_a.PutAll(mask_b);
  return set_a;
}

inline constexpr gpu::SharedImageUsageSet operator|(
    gpu::SharedImageUsage mask_a,
    gpu::SharedImageUsageSet set_b) {
  // Set union is order independent.
  return set_b | mask_a;
}

inline constexpr gpu::SharedImageUsageSet operator|(
    gpu::SharedImageUsage mask_a,
    gpu::SharedImageUsage mask_b) {
  return gpu::SharedImageUsageSet(mask_a) | mask_b;
}

inline constexpr bool operator==(gpu::SharedImageUsageSet set_a,
                                 gpu::SharedImageUsageSet set_b) {
  return set_a.set_storage_ == set_b.set_storage_;
}

// This is used as the debug_label prefix for all shared images created by
// importing buffers in Exo. This prefix is checked in the GPU process when
// reporting if memory for shared images is attributed to exo imports or not.
GPU_EXPORT extern const char kExoTextureLabelPrefix[];

// Returns true if usage is a valid client usage.
GPU_EXPORT bool IsValidClientUsage(SharedImageUsageSet usage);

// Returns true iff usage includes SHARED_IMAGE_USAGE_GLES2_READ or
// SHARED_IMAGE_USAGE_GLES2_WRITE.
GPU_EXPORT bool HasGLES2ReadOrWriteUsage(SharedImageUsageSet usage);

// Create a string to label SharedImageUsage.
GPU_EXPORT std::string CreateLabelForSharedImageUsage(
    SharedImageUsageSet usage);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
