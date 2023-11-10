// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

#ifndef I915_FORMAT_MOD_4_TILED_MTL_MC_CCS
// TODO(b/271455200): Remove this definition once drm_fourcc.h contains it.
/*
 * Intel color control surfaces (CCS) for display ver 14 media compression
 *
 * The main surface is tile4 and at plane index 0, the CCS is linear and
 * at index 1. A 64B CCS cache line corresponds to an area of 4x1 tiles in
 * main surface. In other words, 4 bits in CCS map to a main surface cache
 * line pair. The main surface pitch is required to be a multiple of four
 * tile4 widths. For semi-planar formats like NV12, CCS planes follow the
 * Y and UV planes i.e., planes 0 and 1 are used for Y and UV surfaces,
 * planes 2 and 3 for the respective CCS.
 */
#define I915_FORMAT_MOD_4_TILED_MTL_MC_CCS fourcc_mod_code(INTEL, 14)
#endif

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Rect;
class Size;
class GpuMemoryBuffer;
}  // namespace gfx

namespace media {

class VideoFrame;

// Returns a VideoFrame that wraps a GpuMemoryBuffer that references a
// compressed dma-buf. This is used for Intel media compression in which the
// number of planes for NV12/P010 buffers is 4 instead of 2. If we were to use
// VideoFrame::WrapExternalGpuMemoryBuffer(), we would run into a validation
// error because of the extra planes. Instead of loosening the validation there,
// we introduced WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame() which
// contains this loosened validation. That way, it's easier to reason about the
// safety of the code. A more detailed discussion is available in
// go/brainstorming-for-intel-mmc-on-chrome.
scoped_refptr<VideoFrame> WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    base::TimeDelta timestamp);

// Returns true if |modifier| is known to correspond to the Intel media
// compression feature.
bool IsIntelMediaCompressedModifier(uint64_t modifier);

// Returns the name of an Intel Media Compressed modifier as a string.
std::string IntelMediaCompressedModifierToString(uint64_t modifier);
}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_
