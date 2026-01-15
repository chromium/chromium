// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_command_buffer_traits.h"

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<gfx::GpuMemoryBufferFormatSet>::Write(base::Pickle* m,
                                                       const param_type& p) {
  WriteParam(m, p.ToEnumBitmask());
}

bool ParamTraits<gfx::GpuMemoryBufferFormatSet>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  uint64_t bitmask = 0;
  if (!ReadParam(m, iter, &bitmask)) {
    return false;
  }
  // Check deserialized bitmask contains only bits gfx::GpuMemoryBufferFormatSet
  // expects to be set based on largest enum it expects.
  if (bitmask & ~gfx::GpuMemoryBufferFormatSet::All().ToEnumBitmask()) {
    return false;
  }
  *p = gfx::GpuMemoryBufferFormatSet::FromEnumBitmask(bitmask);
  return true;
}

}  // namespace IPC
