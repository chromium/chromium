// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#include "gpu/ipc/common/gpu_param_traits.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#include "gpu/ipc/common/gpu_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#include "gpu/ipc/common/gpu_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#include "gpu/ipc/common/gpu_param_traits_macros.h"
}  // namespace IPC
