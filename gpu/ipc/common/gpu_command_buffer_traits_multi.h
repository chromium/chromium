// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// Multiply-included message file, hence no include guard here.

#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "ipc/param_traits_macros.h"
#include "ipc/param_traits_utils.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gl/gpu_preference.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_IPC_COMMON_EXPORT

IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    gpu::CommandBufferNamespace,
    gpu::CommandBufferNamespace::INVALID,
    gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES - 1)
IPC_ENUM_TRAITS_MAX_VALUE(gl::GpuPreference, gl::GpuPreference::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::ContextType, gpu::CONTEXT_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::SurfaceOrigin, gfx::SurfaceOrigin::kBottomLeft)
