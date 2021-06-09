// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MESSAGES_H_
#define GPU_IPC_COMMON_GPU_MESSAGES_H_

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <stdint.h>

#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "gpu/ipc/common/gpu_param_traits.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gpu_preference.h"
#include "url/ipc/url_param_traits.h"

#if defined(OS_MAC)
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/mac/io_surface.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

#define IPC_MESSAGE_START GpuChannelMsgStart

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

#if defined(OS_ANDROID)
//------------------------------------------------------------------------------
// Tells the StreamTexture to send its SurfaceTexture to the browser process,
// via the ScopedSurfaceRequestConduit.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_ForwardForSurfaceRequest,
                    base::UnguessableToken)

// Tells the service-side instance to start sending frame available
// notifications.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_StartListening)

// Inform the renderer that a new frame with VulkanYcbcrInfo is available.
IPC_MESSAGE_ROUTED4(GpuStreamTextureMsg_FrameWithInfoAvailable,
                    gpu::Mailbox,
                    gfx::Size /* coded_size */,
                    gfx::Rect /* visible_rect*/,
                    absl::optional<gpu::VulkanYCbCrInfo>)

// Inform the renderer that a new frame is available.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_FrameAvailable)

// Update visible size from MediaPlayer. The size includes rotation of video.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_UpdateRotatedVisibleSize,
                    gfx::Size /* rotated_visible_size */)

#endif

#endif  // GPU_IPC_COMMON_GPU_MESSAGES_H_
