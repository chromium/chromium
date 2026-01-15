// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_
#define GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_

#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "ipc/param_traits.h"
#include "ipc/param_traits_utils.h"

namespace IPC {

template <>
struct GPU_IPC_COMMON_EXPORT ParamTraits<gfx::GpuMemoryBufferFormatSet> {
  typedef gfx::GpuMemoryBufferFormatSet param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

}  // namespace IPC

#endif  // GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_
