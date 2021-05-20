// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MESSAGES_H_
#define GPU_IPC_COMMON_GPU_MESSAGES_H_

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include <stdint.h>

#include <string>
#include <vector>

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

IPC_STRUCT_BEGIN(GPUCommandBufferConsoleMessage)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(std::string, message)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuCommandBufferMsg_CreateImage_Params)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, gpu_memory_buffer)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(gfx::BufferPlane, plane)
  IPC_STRUCT_MEMBER(uint64_t, image_release_count)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(GrSurfaceOrigin, kBottomLeft_GrSurfaceOrigin)

IPC_STRUCT_BEGIN(GpuChannelMsg_CreateGMBSharedImage_Params)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, handle)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(gfx::BufferPlane, plane)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(uint32_t, usage)
  IPC_STRUCT_MEMBER(uint32_t, release_id)
  IPC_STRUCT_MEMBER(GrSurfaceOrigin, surface_origin)
  IPC_STRUCT_MEMBER(SkAlphaType, alpha_type)
IPC_STRUCT_END()

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

IPC_MESSAGE_ROUTED1(GpuChannelMsg_CreateGMBSharedImage,
                    GpuChannelMsg_CreateGMBSharedImage_Params /* params */)

#if defined(OS_FUCHSIA)
IPC_MESSAGE_ROUTED5(GpuChannelMsg_RegisterSysmemBufferCollection,
                    gfx::SysmemBufferCollectionId /* id */,
                    zx::channel /* token */,
                    gfx::BufferFormat /* format */,
                    gfx::BufferUsage /* usage */,
                    bool /* register_with_image_pipe */)
IPC_MESSAGE_ROUTED1(GpuChannelMsg_ReleaseSysmemBufferCollection,
                    gfx::SysmemBufferCollectionId /* id */)
#endif  // OS_FUCHSIA
IPC_MESSAGE_ROUTED1(GpuChannelMsg_RegisterSharedImageUploadBuffer,
                    base::ReadOnlySharedMemoryRegion /* shm */)

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

//------------------------------------------------------------------------------
// GPU Command Buffer Messages
// These are messages between a renderer process to the GPU process relating to
// a single OpenGL context.

// Sets the shared memory buffer used for commands.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetGetBuffer, int32_t /* shm_id */)

// Sent by the GPU process to display messages in the console.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ConsoleMsg,
                    GPUCommandBufferConsoleMessage /* msg */)

// Sent by the GPU process to notify the renderer process of a GPU switch.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_GpuSwitched,
                    gl::GpuPreference /* active_gpu_heuristic */)

// Register an existing shared memory transfer buffer. The id that can be
// used to identify the transfer buffer from a command buffer.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_RegisterTransferBuffer,
                    int32_t /* id */,
                    base::UnsafeSharedMemoryRegion /* transfer_buffer */)

// Tells the proxy that there was an error and the command buffer had to be
// destroyed for some reason.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_Destroyed,
                    gpu::error::ContextLostReason, /* reason */
                    gpu::error::Error /* error */)

// Tells the browser that SwapBuffers returned.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SwapBuffersCompleted,
                    gpu::SwapBuffersCompleteParams /* params */)

// Tells the browser a buffer has been presented on screen.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_BufferPresented,
                    uint64_t, /* swap_id */
                    gfx::PresentationFeedback /* feedback */)

// The receiver will asynchronously wait until the SyncToken is signaled, and
// then return a GpuCommandBufferMsg_SignalAck message.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalSyncToken,
                    gpu::SyncToken /* sync_token */,
                    uint32_t /* signal_id */)

// Makes this command buffer signal when a query is reached, by sending
// back a GpuCommandBufferMsg_SignalSyncPointAck message with the same
// signal_id.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalQuery,
                    uint32_t /* query */,
                    uint32_t /* signal_id */)

// Response to SignalSyncPoint, SignalSyncToken, and SignalQuery.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_SignalAck,
                    uint32_t /* signal_id */,
                    gpu::CommandBuffer::State /* state */)

// Create an image from an existing gpu memory buffer. The id that can be
// used to identify the image from a command buffer.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_CreateImage,
                    GpuCommandBufferMsg_CreateImage_Params /* params */)

// Destroy a previously created image.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_DestroyImage, int32_t /* id */)

// Send a GPU fence handle and store it for the specified gpu fence ID.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_CreateGpuFenceFromHandle,
                    uint32_t /* gpu_fence_id */,
                    gfx::GpuFenceHandle)

// Request retrieval of a GpuFenceHandle by ID.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_GetGpuFenceHandle,
                    uint32_t /* gpu_fence_id */)

// Response to GetGpuFenceHandle.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_GetGpuFenceHandleComplete,
                    uint32_t /* gpu_fence_id */,
                    gfx::GpuFenceHandle)

// Returns a block of data from the GPU process to the renderer.
// This contains server->client messages produced by dawn_wire and is used to
// remote WebGPU.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_ReturnData,
                    std::vector<uint8_t> /* data */)

#endif  // GPU_IPC_COMMON_GPU_MESSAGES_H_
