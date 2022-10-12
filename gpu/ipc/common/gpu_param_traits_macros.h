// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#define GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_

#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/gpu_export.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_STRUCT_TRAITS_BEGIN(gpu::SwapBuffersCompleteParams)
  IPC_STRUCT_TRAITS_MEMBER(ca_layer_params)
  IPC_STRUCT_TRAITS_MEMBER(swap_response)
IPC_STRUCT_TRAITS_END()

#endif  // GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
