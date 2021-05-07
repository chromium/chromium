// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#define GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gpu::SchedulingPriority,
                          gpu::SchedulingPriority::kLast)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::ContextResult,
                          gpu::ContextResult::kLastContextResult)

IPC_STRUCT_TRAITS_BEGIN(gpu::SwapBuffersCompleteParams)
  IPC_STRUCT_TRAITS_MEMBER(ca_layer_params)
  IPC_STRUCT_TRAITS_MEMBER(texture_in_use_responses)
  IPC_STRUCT_TRAITS_MEMBER(swap_response)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(viz::ResourceFormat, viz::RESOURCE_FORMAT_MAX)

#endif  // GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
