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

#include "base/optional.h"
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

IPC_STRUCT_BEGIN(GPUCreateCommandBufferConfig)
  IPC_STRUCT_MEMBER(gpu::SurfaceHandle, surface_handle)
  IPC_STRUCT_MEMBER(int32_t, share_group_id)
  IPC_STRUCT_MEMBER(int32_t, stream_id)
  IPC_STRUCT_MEMBER(gpu::SchedulingPriority, stream_priority)
  IPC_STRUCT_MEMBER(gpu::ContextCreationAttribs, attribs)
  IPC_STRUCT_MEMBER(GURL, active_url)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuCommandBufferMsg_CreateImage_Params)
  IPC_STRUCT_MEMBER(int32_t, id)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, gpu_memory_buffer)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(uint64_t, image_release_count)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(GrSurfaceOrigin, kBottomLeft_GrSurfaceOrigin)

IPC_STRUCT_BEGIN(GpuChannelMsg_CreateSharedImage_Params)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(viz::ResourceFormat, format)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(uint32_t, usage)
  IPC_STRUCT_MEMBER(uint32_t, release_id)
  IPC_STRUCT_MEMBER(GrSurfaceOrigin, surface_origin)
  IPC_STRUCT_MEMBER(SkAlphaType, alpha_type)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuChannelMsg_CreateSharedImageWithData_Params)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(viz::ResourceFormat, format)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(uint32_t, usage)
  IPC_STRUCT_MEMBER(uint32_t, release_id)
  IPC_STRUCT_MEMBER(uint32_t, pixel_data_offset)
  IPC_STRUCT_MEMBER(uint32_t, pixel_data_size)
  IPC_STRUCT_MEMBER(bool, done_with_shm)
  IPC_STRUCT_MEMBER(GrSurfaceOrigin, surface_origin)
  IPC_STRUCT_MEMBER(SkAlphaType, alpha_type)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuChannelMsg_CreateGMBSharedImage_Params)
  IPC_STRUCT_MEMBER(gpu::Mailbox, mailbox)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferHandle, handle)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(uint32_t, usage)
  IPC_STRUCT_MEMBER(uint32_t, release_id)
  IPC_STRUCT_MEMBER(GrSurfaceOrigin, surface_origin)
  IPC_STRUCT_MEMBER(SkAlphaType, alpha_type)
IPC_STRUCT_END()

#if defined(OS_WIN)
IPC_STRUCT_BEGIN(GpuChannelMsg_CreateSwapChain_Params)
  IPC_STRUCT_MEMBER(gpu::Mailbox, front_buffer_mailbox)
  IPC_STRUCT_MEMBER(gpu::Mailbox, back_buffer_mailbox)
  IPC_STRUCT_MEMBER(viz::ResourceFormat, format)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, color_space)
  IPC_STRUCT_MEMBER(uint32_t, usage)
  IPC_STRUCT_MEMBER(uint32_t, release_id)
  IPC_STRUCT_MEMBER(GrSurfaceOrigin, surface_origin)
  IPC_STRUCT_MEMBER(SkAlphaType, alpha_type)
IPC_STRUCT_END()
#endif  // OS_WIN

IPC_STRUCT_BEGIN(GpuChannelMsg_ScheduleImageDecode_Params)
  IPC_STRUCT_MEMBER(std::vector<uint8_t>, encoded_data)
  IPC_STRUCT_MEMBER(gfx::Size, output_size)
  IPC_STRUCT_MEMBER(int32_t, raster_decoder_route_id)
  IPC_STRUCT_MEMBER(uint32_t, transfer_cache_entry_id)
  IPC_STRUCT_MEMBER(int32_t, discardable_handle_shm_id)
  IPC_STRUCT_MEMBER(uint32_t, discardable_handle_shm_offset)
  IPC_STRUCT_MEMBER(uint64_t, discardable_handle_release_count)
  IPC_STRUCT_MEMBER(gfx::ColorSpace, target_color_space)
  IPC_STRUCT_MEMBER(bool, needs_mips)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(GpuDeferredMessage)
  IPC_STRUCT_MEMBER(IPC::Message, message)
  IPC_STRUCT_MEMBER(std::vector<gpu::SyncToken>, sync_token_fences)
IPC_STRUCT_END()

//------------------------------------------------------------------------------
// GPU Channel Messages
// These are messages from a renderer process to the GPU process.

// Tells the GPU process to create a new command buffer. A corresponding
// CommandBufferStub is created.  If |surface_handle| is non-null, |size|
// is ignored, and it will render directly to the native surface (only the
// browser process is allowed to create those). Otherwise it will create an
// offscreen backbuffer of dimensions |size|.
IPC_SYNC_MESSAGE_CONTROL3_2(GpuChannelMsg_CreateCommandBuffer,
                            GPUCreateCommandBufferConfig /* init_params */,
                            int32_t /* route_id */,
                            base::UnsafeSharedMemoryRegion /* shared_state */,
                            gpu::ContextResult,
                            gpu::Capabilities /* capabilities */)

// The CommandBufferProxy sends this to the CommandBufferStub in its
// destructor, so that the stub deletes the actual CommandBufferService
// object that it's hosting.
IPC_SYNC_MESSAGE_CONTROL1_0(GpuChannelMsg_DestroyCommandBuffer,
                            int32_t /* instance_id */)

IPC_MESSAGE_CONTROL1(GpuChannelMsg_FlushDeferredMessages,
                     std::vector<GpuDeferredMessage> /* deferred_messages */)

IPC_MESSAGE_ROUTED1(GpuChannelMsg_CreateSharedImage,
                    GpuChannelMsg_CreateSharedImage_Params /* params */)
IPC_MESSAGE_ROUTED1(GpuChannelMsg_CreateSharedImageWithData,
                    GpuChannelMsg_CreateSharedImageWithData_Params /* params */)
IPC_MESSAGE_ROUTED1(GpuChannelMsg_CreateGMBSharedImage,
                    GpuChannelMsg_CreateGMBSharedImage_Params /* params */)

// The following IPC message, that can be used by the browser or renderers,
// updates the SharedImage referenced by |id| after its contents are modified
// (e.g: its GpuMemoryBuffer is modified via the CPU or through external
// devices).
// The sync token in the shared image sequence at position |release_id| will be
// released once this command has been executed service side.
// |in_fence_handle|, if not null, represents a fence that will be waited on
// before reading the contents represented by the shared image.
IPC_MESSAGE_ROUTED3(GpuChannelMsg_UpdateSharedImage,
                    gpu::Mailbox /* id */,
                    uint32_t /* release_id */,
                    gfx::GpuFenceHandle /* in_fence_handle */)
#if defined(OS_ANDROID)
IPC_MESSAGE_ROUTED4(GpuChannelMsg_CreateSharedImageWithAHB,
                    gpu::Mailbox /* out id */,
                    gpu::Mailbox /* in id */,
                    uint32_t /* usage */,
                    uint32_t /* release_id */)
#endif
IPC_MESSAGE_ROUTED1(GpuChannelMsg_DestroySharedImage, gpu::Mailbox /* id */)
#if defined(OS_WIN)
IPC_MESSAGE_ROUTED1(GpuChannelMsg_CreateSwapChain,
                    GpuChannelMsg_CreateSwapChain_Params /* params */)
IPC_MESSAGE_ROUTED2(GpuChannelMsg_PresentSwapChain,
                    gpu::Mailbox /* mailbox */,
                    uint32_t /* release_id */)
#endif  // OS_WIN
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

// Schedules a hardware-accelerated image decode in the GPU process. Renderers
// should use gpu::ImageDecodeAcceleratorProxy to schedule decode requests which
// are processed by gpu::ImageDecodeAcceleratorStub on the service side.
IPC_MESSAGE_ROUTED2(
    GpuChannelMsg_ScheduleImageDecode,
    GpuChannelMsg_ScheduleImageDecode_Params /* decode_params */,
    uint64_t /* decode_release_count */)

// Crash the GPU process in similar way to how chrome://gpucrash does.
// This is only supported in testing environments, and is otherwise ignored.
IPC_MESSAGE_CONTROL0(GpuChannelMsg_CrashForTesting)

// Terminates the GPU process with an exit code of 0. This message is handled in
// in GpuChannelMessageFilter::OnMessageReceived and is only used in tests where
// the GPU benchmarking extension is enabled. The purpose of this API is to test
// scenarios where the GPU process is terminated on purpose with exit code of 0.
IPC_MESSAGE_CONTROL0(GpuChannelMsg_TerminateForTesting)

// Simple NOP message which can be used as fence to ensure all previous sent
// messages have been received.
IPC_SYNC_MESSAGE_CONTROL0_0(GpuChannelMsg_Nop)

// Creates a StreamTexture attached to the provided |stream_id|.
IPC_SYNC_MESSAGE_CONTROL1_1(GpuChannelMsg_CreateStreamTexture,
                            int32_t, /* stream_id */
                            bool /* succeeded */)

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
                    base::Optional<gpu::VulkanYCbCrInfo>)

// Inform the renderer that a new frame is available.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_FrameAvailable)

// Update visible size from MediaPlayer. The size includes rotation of video.
IPC_MESSAGE_ROUTED1(GpuStreamTextureMsg_UpdateRotatedVisibleSize,
                    gfx::Size /* rotated_visible_size */)

// Destroys the StreamTexture attached to the provided |stream_id|.
IPC_MESSAGE_ROUTED0(GpuStreamTextureMsg_Destroy)

#endif

//------------------------------------------------------------------------------
// GPU Command Buffer Messages
// These are messages between a renderer process to the GPU process relating to
// a single OpenGL context.

// Sets the shared memory buffer used for commands.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_SetGetBuffer, int32_t /* shm_id */)

// Takes the front buffer into a mailbox. This allows another context to draw
// the output of this context.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_TakeFrontBuffer,
                    gpu::Mailbox /* mailbox */)

// Returns a front buffer taken with GpuCommandBufferMsg_TakeFrontBuffer. This
// allows it to be reused.
IPC_MESSAGE_ROUTED2(GpuCommandBufferMsg_ReturnFrontBuffer,
                    gpu::Mailbox /* mailbox */,
                    bool /* is_lost */)

// Wait until the token is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED2_1(GpuCommandBufferMsg_WaitForTokenInRange,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Wait until the get offset is in a specific range, inclusive.
IPC_SYNC_MESSAGE_ROUTED3_1(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                           uint32_t /* set_get_buffer_count */,
                           int32_t /* start */,
                           int32_t /* end */,
                           gpu::CommandBuffer::State /* state */)

// Asynchronously synchronize the put and get offsets of both processes.
// Caller passes its current put offset. Current state (including get offset)
// is returned in shared memory.
// TODO(sunnyps): This is an internal implementation detail of the gpu service
// and is not sent by the client. Remove this once the non-scheduler code path
// is removed.
IPC_MESSAGE_ROUTED3(GpuCommandBufferMsg_AsyncFlush,
                    int32_t /* put_offset */,
                    uint32_t /* flush_id */,
                    std::vector<gpu::SyncToken> /* sync_token_fences */)

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

// Destroy a previously created transfer buffer.
IPC_MESSAGE_ROUTED1(GpuCommandBufferMsg_DestroyTransferBuffer, int32_t /* id */)

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
