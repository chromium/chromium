// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <algorithm>
#include <vector>

#include <dawn/dawn_proc.h>

#include "base/numerics/checked_math.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/dawn_client_serializer.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"

#define GPU_CLIENT_SINGLE_THREAD_CHECK()

namespace gpu {
namespace webgpu {

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_impl_autogen.h"

WebGPUImplementation::WebGPUImplementation(
    WebGPUCmdHelper* helper,
    TransferBufferInterface* transfer_buffer,
    GpuControl* gpu_control)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper) {}

WebGPUImplementation::~WebGPUImplementation() {
#if BUILDFLAG(USE_DAWN)
  if (!wire_client_) {
    // Initialization failed.
    return;
  }

  // Flush all commands and synchronously wait for them to finish.
  // TODO(enga): Investigate if we can just Disconnect() instead.
  wire_serializer_->Flush();
  helper_->Finish();

  // Now that commands are finished, free the wire and serializers.
  wire_client_.reset();
  wire_serializer_.reset();

  // All client-side Dawn objects are now destroyed.
  // Shared memory allocations for buffers that were still mapped at the time
  // of destruction can now be safely freed.
  memory_transfer_service_->FreeHandles(helper_);
  helper_->Finish();
#endif
}

gpu::ContextResult WebGPUImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "WebGPUImplementation::Initialize");
  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

#if BUILDFLAG(USE_DAWN)
  memory_transfer_service_ =
      std::make_unique<DawnClientMemoryTransferService>(mapped_memory_.get());

  wire_serializer_ = DawnClientSerializer::Create(
      this, helper_, memory_transfer_service_.get(), limits);

  dawn_wire::WireClientDescriptor descriptor = {};
  descriptor.serializer = wire_serializer_.get();
  descriptor.memoryTransferService = memory_transfer_service_.get();
  wire_client_ = std::make_unique<dawn_wire::WireClient>(descriptor);

  // TODO(senorblanco): Do this only once per process. Doing it once per
  // WebGPUImplementation is non-optimal but valid, since the returned
  // procs are always the same.
  dawnProcSetProcs(&dawn_wire::client::GetProcs());
#endif

  return gpu::ContextResult::kSuccess;
}

// ContextSupport implementation.
void WebGPUImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::Swap(uint32_t flags,
                                SwapCompletedCallback complete_callback,
                                PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::SwapWithBounds(
    const std::vector<gfx::Rect>& rects,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::PartialSwapBuffers(
    const gfx::Rect& sub_buffer,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::CommitOverlayPlanes(
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTREACHED();
}
void WebGPUImplementation::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect,
    bool enable_blend,
    unsigned gpu_fence_id) {
  NOTREACHED();
}
uint64_t WebGPUImplementation::ShareGroupTracingGUID() const {
  NOTIMPLEMENTED();
  return 0;
}
void WebGPUImplementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  NOTIMPLEMENTED();
}
bool WebGPUImplementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void* WebGPUImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED();
  return nullptr;
}
void WebGPUImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED();
}
void WebGPUImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  NOTREACHED();
}
unsigned int WebGPUImplementation::GetTransferBufferFreeSize() const {
  NOTREACHED();
  return 0;
}
bool WebGPUImplementation::IsJpegDecodeAccelerationSupported() const {
  NOTREACHED();
  return false;
}
bool WebGPUImplementation::IsWebPDecodeAccelerationSupported() const {
  NOTREACHED();
  return false;
}
bool WebGPUImplementation::CanDecodeWithHardwareAcceleration(
    const cc::ImageHeaderMetadata* image_metadata) const {
  NOTREACHED();
  return false;
}

// InterfaceBase implementation.
void WebGPUImplementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenSyncToken(sync_token);
}
void WebGPUImplementation::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenUnverifiedSyncToken(sync_token);
}
void WebGPUImplementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                    GLsizei count) {
  ImplementationBase::VerifySyncTokens(sync_tokens, count);
}
void WebGPUImplementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  // Flush any commands before this, so we don't block more than necessary.
  FlushCommands();
  ImplementationBase::WaitSyncToken(sync_token);
}

bool WebGPUImplementation::HasGrContextSupport() const {
  return true;
}

// ImplementationBase implementation.
void WebGPUImplementation::IssueShallowFlush() {
  NOTIMPLEMENTED();
}

void WebGPUImplementation::SetGLError(GLenum error,
                                      const char* function_name,
                                      const char* msg) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] Client Synthesized Error: "
                     << gles2::GLES2Util::GetStringError(error) << ": "
                     << function_name << ": " << msg);
  NOTIMPLEMENTED();
}

// GpuControlClient implementation.
// TODO(jiawei.shao@intel.com): do other clean-ups when the context is lost.
void WebGPUImplementation::OnGpuControlLostContext() {
  OnGpuControlLostContextMaybeReentrant();

  // This should never occur more than once.
  DCHECK(!lost_context_callback_run_);
  lost_context_callback_run_ = true;
  if (!lost_context_callback_.is_null()) {
    std::move(lost_context_callback_).Run();
  }
}
void WebGPUImplementation::OnGpuControlLostContextMaybeReentrant() {
  lost_ = true;
#if BUILDFLAG(USE_DAWN)
  wire_client_->Disconnect();
  wire_serializer_->Disconnect();
#endif
}
void WebGPUImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnSwapBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  if (lost_) {
    return;
  }
#if BUILDFLAG(USE_DAWN)

  static uint32_t return_trace_id = 0;
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                         "DawnReturnCommands", TRACE_EVENT_FLAG_FLOW_IN,
                         return_trace_id++);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUImplementation::OnGpuControlReturnData", "bytes",
               data.size());

  CHECK_GT(data.size(), sizeof(cmds::DawnReturnDataHeader));

  const cmds::DawnReturnDataHeader& dawnReturnDataHeader =
      *reinterpret_cast<const cmds::DawnReturnDataHeader*>(data.data());

  switch (dawnReturnDataHeader.return_data_type) {
    case DawnReturnDataType::kDawnCommands: {
      CHECK_GE(data.size(), sizeof(cmds::DawnReturnCommandsInfo));

      const cmds::DawnReturnCommandsInfo* dawn_return_commands_info =
          reinterpret_cast<const cmds::DawnReturnCommandsInfo*>(data.data());
      // TODO(enga): Instead of a CHECK, this could generate a device lost
      // event on just that device. It doesn't seem worth doing right now
      // since a failure here is likely not recoverable.
      CHECK(wire_client_->HandleCommands(
          reinterpret_cast<const char*>(
              dawn_return_commands_info->deserialized_buffer),
          data.size() -
              offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer)));
    } break;
    case DawnReturnDataType::kRequestedDawnAdapterProperties: {
      CHECK_GE(data.size(), sizeof(cmds::DawnReturnAdapterInfo));

      const cmds::DawnReturnAdapterInfo* returned_adapter_info =
          reinterpret_cast<const cmds::DawnReturnAdapterInfo*>(data.data());

      DawnRequestAdapterSerial request_adapter_serial =
          returned_adapter_info->header.request_adapter_serial;
      auto request_callback_iter =
          request_adapter_callback_map_.find(request_adapter_serial);
      CHECK(request_callback_iter != request_adapter_callback_map_.end());

      auto& request_callback = request_callback_iter->second;
      GLuint adapter_service_id =
          returned_adapter_info->header.adapter_service_id;
      WGPUDeviceProperties adapter_properties = {};
      const volatile char* deserialized_buffer =
          reinterpret_cast<const volatile char*>(
              returned_adapter_info->deserialized_buffer);
      const char* error_message =
          returned_adapter_info->deserialized_buffer +
          returned_adapter_info->adapter_properties_size;
      if (strlen(error_message) == 0) {
        error_message = nullptr;
      }
      if (returned_adapter_info->adapter_properties_size > 0) {
        if (!dawn_wire::DeserializeWGPUDeviceProperties(
                &adapter_properties,
                deserialized_buffer,
                returned_adapter_info->adapter_properties_size)) {
          adapter_service_id = -1;
          adapter_properties = {};
          error_message = "Request adapter failed";
        }
      }
      std::move(request_callback)
          .Run(adapter_service_id, adapter_properties, error_message);
      request_adapter_callback_map_.erase(request_callback_iter);
    } break;
    case DawnReturnDataType::kRequestedDeviceReturnInfo: {
      CHECK_GE(data.size(), sizeof(cmds::DawnReturnRequestDeviceInfo));

      const cmds::DawnReturnRequestDeviceInfo* returned_request_device_info =
          reinterpret_cast<const cmds::DawnReturnRequestDeviceInfo*>(
              data.data());

      DawnRequestDeviceSerial request_device_serial =
          returned_request_device_info->request_device_serial;
      auto request_callback_iter =
          request_device_callback_map_.find(request_device_serial);
      CHECK(request_callback_iter != request_device_callback_map_.end());

      auto& request_callback = request_callback_iter->second;
      bool is_request_device_success =
          returned_request_device_info->is_request_device_success;
      std::move(request_callback).Run(is_request_device_success);
      request_device_callback_map_.erase(request_callback_iter);
    } break;
    default:
      NOTREACHED();
  }
#endif
}

const DawnProcTable& WebGPUImplementation::GetProcs() const {
#if !BUILDFLAG(USE_DAWN)
  NOTREACHED();
  static DawnProcTable null_procs = {};
  return null_procs;
#else
  return dawn_wire::client::GetProcs();
#endif
}

void WebGPUImplementation::FlushCommands() {
#if BUILDFLAG(USE_DAWN)
  wire_serializer_->Flush();
  helper_->Flush();
#endif
}

void WebGPUImplementation::EnsureAwaitingFlush(bool* needs_flush) {
#if BUILDFLAG(USE_DAWN)
  // If there is already a flush waiting, we don't need to flush.
  // We only want to set |needs_flush| on state transition from
  // false -> true.
  if (wire_serializer_->AwaitingFlush()) {
    *needs_flush = false;
    return;
  }

  // Set the state to waiting for flush, and then write |needs_flush|.
  // Could still be false if there's no data to flush.
  wire_serializer_->SetAwaitingFlush(true);
  *needs_flush = wire_serializer_->AwaitingFlush();
#else
  *needs_flush = false;
#endif
}

void WebGPUImplementation::FlushAwaitingCommands() {
#if BUILDFLAG(USE_DAWN)
  if (wire_serializer_->AwaitingFlush()) {
    wire_serializer_->Flush();
    helper_->Flush();
  }
#endif
}

void WebGPUImplementation::DisconnectContextAndDestroyServer() {
  // Treat this like a context lost since the context is no longer usable.
  // TODO(crbug.com/1160459): Also send a message to eagerly free server-side
  // resources.
  OnGpuControlLostContextMaybeReentrant();
}

ReservedTexture WebGPUImplementation::ReserveTexture(WGPUDevice device) {
#if BUILDFLAG(USE_DAWN)
  // Flush because we need to make sure messages that free a previously used
  // texture are seen first. ReserveTexture may reuse an existing ID.
  wire_serializer_->Flush();

  auto reservation = wire_client_->ReserveTexture(device);
  ReservedTexture result;
  result.texture = reservation.texture;
  result.id = reservation.id;
  result.generation = reservation.generation;
  result.deviceId = reservation.deviceId;
  result.deviceGeneration = reservation.deviceGeneration;
  return result;
#else
  NOTREACHED();
  return {};
#endif
}

DawnRequestAdapterSerial WebGPUImplementation::NextRequestAdapterSerial() {
  return ++request_adapter_serial_;
}

void WebGPUImplementation::RequestAdapterAsync(
    PowerPreference power_preference,
    base::OnceCallback<void(int32_t, const WGPUDeviceProperties&, const char*)>
        request_adapter_callback) {
  if (lost_) {
    std::move(request_adapter_callback).Run(-1, {}, "Context Lost");
    return;
  }

  // Now that we declare request_adapter_serial as an uint64, it can't overflow
  // because we just increment an uint64 by one.
  DawnRequestAdapterSerial request_adapter_serial = NextRequestAdapterSerial();
  DCHECK(request_adapter_callback_map_.find(request_adapter_serial) ==
         request_adapter_callback_map_.end());

  request_adapter_callback_map_[request_adapter_serial] =
      std::move(request_adapter_callback);

  helper_->RequestAdapter(request_adapter_serial,
                          static_cast<uint32_t>(power_preference));
  helper_->Flush();
}

DawnRequestDeviceSerial WebGPUImplementation::NextRequestDeviceSerial() {
  return ++request_device_serial_;
}

void WebGPUImplementation::RequestDeviceAsync(
    uint32_t requested_adapter_id,
    const WGPUDeviceProperties& requested_device_properties,
    base::OnceCallback<void(WGPUDevice)> request_device_callback) {
#if BUILDFLAG(USE_DAWN)
  if (lost_) {
    std::move(request_device_callback).Run(nullptr);
    return;
  }

  size_t serialized_device_properties_size =
      dawn_wire::SerializedWGPUDevicePropertiesSize(
          &requested_device_properties);
  DCHECK_NE(0u, serialized_device_properties_size);

  DCHECK_LE(serialized_device_properties_size, transfer_buffer_->GetMaxSize());

  ScopedTransferBufferPtr buffer(serialized_device_properties_size, helper_,
                                 transfer_buffer_);

  if (!buffer.valid() || buffer.size() < serialized_device_properties_size) {
    std::move(request_device_callback).Run(nullptr);
    return;
  }

  // We declare DawnRequestDeviceSerial as an uint64, so it can't overflow
  // because we just increment an uint64 by one.
  DawnRequestDeviceSerial request_device_serial = NextRequestDeviceSerial();
  DCHECK(request_device_callback_map_.find(request_device_serial) ==
         request_device_callback_map_.end());

  // Flush because we need to make sure messages that free a previously used
  // device are seen first. ReserveDevice may reuse an existing ID.
  wire_serializer_->Flush();

  dawn_wire::ReservedDevice reservation = wire_client_->ReserveDevice();

  request_device_callback_map_[request_device_serial] = base::BindOnce(
      [](dawn_wire::WireClient* wire_client,
         dawn_wire::ReservedDevice reservation,
         base::OnceCallback<void(WGPUDevice)> callback, bool success) {
        WGPUDevice device = reservation.device;
        if (!success) {
          wire_client->ReclaimDeviceReservation(reservation);
          device = nullptr;
        }
        std::move(callback).Run(device);
      },
      wire_client_.get(), reservation, std::move(request_device_callback));

  dawn_wire::SerializeWGPUDeviceProperties(
      &requested_device_properties, reinterpret_cast<char*>(buffer.address()));

  helper_->RequestDevice(request_device_serial, requested_adapter_id,
                         reservation.id, reservation.generation,
                         buffer.shm_id(), buffer.offset(),
                         serialized_device_properties_size);
  buffer.Release();
  helper_->Flush();
#endif
}

WGPUDevice WebGPUImplementation::DeprecatedEnsureDefaultDeviceSync() {
  if (deprecated_default_device_ != nullptr) {
    return deprecated_default_device_;
  }

  DLOG(WARNING) << "Using deprecated WebGPU device initialization";

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  RequestAdapterAsync(
      PowerPreference::kDefault,
      base::BindOnce(
          [](WebGPUImplementation* self, WGPUDevice* result,
             base::OnceCallback<void()> done, int32_t adapter_id,
             const WGPUDeviceProperties& properties, const char* name) {
            if (adapter_id < 0) {
              std::move(done).Run();
              return;
            }
            self->RequestDeviceAsync(
                static_cast<uint32_t>(adapter_id), properties,
                base::BindOnce(
                    [](WGPUDevice* result, base::OnceCallback<void()> done,
                       WGPUDevice device) {
                      *result = device;
                      std::move(done).Run();
                    },
                    result, std::move(done)));
          },
          this, &deprecated_default_device_, run_loop.QuitClosure()));
  run_loop.Run();

  return deprecated_default_device_;
}

void WebGPUImplementation::AssociateMailbox(GLuint device_id,
                                            GLuint device_generation,
                                            GLuint texture_id,
                                            GLuint texture_generation,
                                            GLuint usage,
                                            const GLbyte* mailbox) {
#if BUILDFLAG(USE_DAWN)
  // Flush previous Dawn commands as they may manipulate texture object IDs
  // and need to be resolved prior to the AssociateMailbox command. Otherwise
  // the service side might not know, for example that the previous texture
  // using that ID has been released.
  wire_serializer_->Flush();
  helper_->AssociateMailboxImmediate(device_id, device_generation, texture_id,
                                     texture_generation, usage, mailbox);
#endif
}

void WebGPUImplementation::DissociateMailbox(GLuint texture_id,
                                             GLuint texture_generation) {
#if BUILDFLAG(USE_DAWN)
  // Flush previous Dawn commands that might be rendering to the texture, prior
  // to Dissociating the shared image from that texture.
  wire_serializer_->Flush();
  helper_->DissociateMailbox(texture_id, texture_generation);
#endif
}

}  // namespace webgpu
}  // namespace gpu
