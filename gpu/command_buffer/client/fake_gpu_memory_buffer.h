// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_

#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// This method is used by tests to create a fake pixmap handle instead of
// creating a FakeGpuMemoryBuffer. Once all tests are converted to use it,
// FakeGpuMemoryBuffer will be removed and this file will be renamed
// appropriately. Note that this method is only exposed to linux and chromeos
// whereas the FakeGpuMemoryBuffer itself can be used in any platform as of now
// with a handle type of gfx::NATIVE_PIXMAP which is confusing. Removing
// and replacing FakeGpuMemoryBuffer with platform specific handle creation
// methods will address those concerns.
gfx::GpuMemoryBufferHandle CreatePixmapHandleForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_
