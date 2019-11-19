// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_

#include <stddef.h>

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

struct SharedMemoryLimits {
  SharedMemoryLimits() {
// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !defined(OS_NACL)
    // Max mapped memory to use for a texture upload depends on device ram.
    // Do not use more than 5% of extra shared memory, and do not use any extra
    // for memory contrained devices (<=1GB).
    max_mapped_memory_for_texture_upload =
        base::SysInfo::AmountOfPhysicalMemory() > 1024 * 1024 * 1024
            ? base::saturated_cast<uint32_t>(
                  base::SysInfo::AmountOfPhysicalMemory() / 20)
            : 0;

    // On memory constrained devices, switch to lower limits.
    if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512) {
      command_buffer_size = 512 * 1024;
      start_transfer_buffer_size = 32 * 1024;
      min_transfer_buffer_size = 32 * 1024;
      mapped_memory_chunk_size = 256 * 1024;
    }
#endif
  }

  uint32_t command_buffer_size = 1024 * 1024;
  uint32_t start_transfer_buffer_size = 64 * 1024;
  uint32_t min_transfer_buffer_size = 64 * 1024;
  uint32_t max_transfer_buffer_size = 16 * 1024 * 1024;

  static constexpr uint32_t kNoLimit = 0;
  uint32_t mapped_memory_reclaim_limit = kNoLimit;
  uint32_t mapped_memory_chunk_size = 2 * 1024 * 1024;
  uint32_t max_mapped_memory_for_texture_upload = 0;

  // These are limits for contexts only used for creating textures, mailboxing
  // them and dealing with synchronization.
  static SharedMemoryLimits ForMailboxContext() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = 64 * 1024;
    limits.start_transfer_buffer_size = 64 * 1024;
    limits.min_transfer_buffer_size = 64 * 1024;
    return limits;
  }

  static SharedMemoryLimits ForOOPRasterContext() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = 64 * 1024;
    // TODO(khushalsagar): See if transfer buffer sizes can be fine-tuned
    // further. A 16M max_transfer_buffer_size doesn't make sense if only paint
    // commands are being sent through this buffer, and all large transfers use
    // the transfer cache backed by mapped memory.
    return limits;
  }

  static SharedMemoryLimits ForWebGPUContext() {
    // Most WebGPU commands are sent via transfer buffer, so we use a smaller
    // command buffer.
    SharedMemoryLimits limits;
    limits.command_buffer_size = 64 * 1024;

    return limits;
  }

#if defined(OS_ANDROID)
  static SharedMemoryLimits ForDisplayCompositor(const gfx::Size& screen_size) {
    DCHECK(!screen_size.IsEmpty());

    SharedMemoryLimits limits;
    constexpr uint32_t kBytesPerPixel = 4;
    const uint32_t full_screen_texture_size_in_bytes =
        screen_size.width() * screen_size.height() * kBytesPerPixel;

    // Android uses a smaller command buffer for the display compositor. Meant
    // to hold the contents of the display compositor drawing the scene. See
    // discussion here: https://goo.gl/s23m5j
    limits.command_buffer_size = 64 * 1024;
    // These limits are meant to hold the uploads for the browser UI without
    // any excess space.
    limits.start_transfer_buffer_size = 64 * 1024;
    limits.min_transfer_buffer_size = 64 * 1024;
    limits.max_transfer_buffer_size = full_screen_texture_size_in_bytes;
    // Texture uploads may use mapped memory so give a reasonable limit for
    // them.
    limits.mapped_memory_reclaim_limit = full_screen_texture_size_in_bytes;

    return limits;
  }
#else
  static SharedMemoryLimits ForDisplayCompositor() {
    return SharedMemoryLimits();
  }
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_
