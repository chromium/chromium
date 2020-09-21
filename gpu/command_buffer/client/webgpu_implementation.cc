// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <algorithm>
#include <vector>

#include <dawn/dawn_proc.h>

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"

#define GPU_CLIENT_SINGLE_THREAD_CHECK()

namespace gpu {
namespace webgpu {

#if BUILDFLAG(USE_DAWN)
WebGPUCommandSerializer::WebGPUCommandSerializer(
    DawnDeviceClientID device_client_id,
    WebGPUCmdHelper* helper,
    DawnClientMemoryTransferService* memory_transfer_service,
    std::unique_ptr<TransferBuffer> c2s_transfer_buffer)
    : device_client_id_(device_client_id),
      helper_(helper),
      memory_transfer_service_(memory_transfer_service),
      c2s_transfer_buffer_(std::move(c2s_transfer_buffer)),
      c2s_buffer_(helper_, c2s_transfer_buffer_.get()) {
  DCHECK(helper_);
  DCHECK(c2s_transfer_buffer_ && c2s_transfer_buffer_->HaveBuffer());

  const SharedMemoryLimits& limits = SharedMemoryLimits::ForWebGPUContext();
  c2s_buffer_default_size_ = limits.start_transfer_buffer_size;
  DCHECK_GT(c2s_buffer_default_size_, 0u);

  DCHECK(memory_transfer_service_);
  dawn_wire::WireClientDescriptor descriptor = {};
  descriptor.serializer = this;
  descriptor.memoryTransferService = memory_transfer_service_;
  wire_client_ = std::make_unique<dawn_wire::WireClient>(descriptor);
}

WebGPUCommandSerializer::~WebGPUCommandSerializer() {
  // Destroy the wire client before anything else because it might still call
  // GetCmdSpace so the rest of the serializer must still be valid.
  wire_client_ = nullptr;
}

// This function can only be called once for each WebGPUCommandSerializer
// object (before any call of GetCmdSpace()).
void WebGPUCommandSerializer::RequestDeviceCreation(
    uint32_t requested_adapter_id,
    const WGPUDeviceProperties& requested_device_properties) {
  DCHECK(!c2s_buffer_.valid());
  DCHECK_EQ(0u, c2s_put_offset_);

  size_t serialized_device_properties_size =
      dawn_wire::SerializedWGPUDevicePropertiesSize(
          &requested_device_properties);
  DCHECK_NE(0u, serialized_device_properties_size);

  DCHECK_LE(serialized_device_properties_size,
            c2s_transfer_buffer_->GetMaxSize());
  c2s_buffer_.Reset(serialized_device_properties_size);

  dawn_wire::SerializeWGPUDeviceProperties(
      &requested_device_properties,
      reinterpret_cast<char*>(c2s_buffer_.address()));

  helper_->RequestDevice(device_client_id_, requested_adapter_id,
                         c2s_buffer_.shm_id(), c2s_buffer_.offset(),
                         serialized_device_properties_size);
  c2s_buffer_.Release();

  helper_->Flush();
}

void* WebGPUCommandSerializer::GetCmdSpace(size_t size) {
  // The buffer size must be initialized before any commands are serialized.
  if (c2s_buffer_default_size_ == 0u) {
    NOTREACHED();
    return nullptr;
  }

  base::CheckedNumeric<uint32_t> checked_next_offset(c2s_put_offset_);
  checked_next_offset += size;

  uint32_t next_offset;
  bool next_offset_valid = checked_next_offset.AssignIfValid(&next_offset);

  // If the buffer does not have enough space, or if the buffer is not
  // initialized, flush and reset the command stream.
  if (!next_offset_valid || next_offset > c2s_buffer_.size() ||
      !c2s_buffer_.valid()) {
    Flush();

    uint32_t max_allocation = c2s_transfer_buffer_->GetMaxSize();
    // TODO(crbug.com/951558): Handle command chunking or ensure commands aren't
    // this large.
    CHECK_LE(size, max_allocation);

    uint32_t allocation_size =
        std::max(c2s_buffer_default_size_, static_cast<uint32_t>(size));
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUCommandSerializer::GetCmdSpace", "bytes",
                 allocation_size);
    c2s_buffer_.Reset(allocation_size);
    c2s_put_offset_ = 0;
    next_offset = size;

    // TODO(crbug.com/951558): Handle OOM.
    CHECK(c2s_buffer_.valid());
    CHECK_LE(size, c2s_buffer_.size());
  }

  DCHECK(c2s_buffer_.valid());
  uint8_t* ptr = static_cast<uint8_t*>(c2s_buffer_.address());
  ptr += c2s_put_offset_;

  c2s_put_offset_ = next_offset;
  return ptr;
}

bool WebGPUCommandSerializer::Flush() {
  if (c2s_buffer_.valid()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUCommandSerializer::Flush", "bytes", c2s_put_offset_);

    TRACE_EVENT_FLOW_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
        (static_cast<uint64_t>(c2s_buffer_.shm_id()) << 32) +
            c2s_buffer_.offset());

    c2s_buffer_.Shrink(c2s_put_offset_);
    helper_->DawnCommands(device_client_id_, c2s_buffer_.shm_id(),
                          c2s_buffer_.offset(), c2s_put_offset_);
    c2s_put_offset_ = 0;
    c2s_buffer_.Release();
    client_awaiting_flush_ = false;
  }

  memory_transfer_service_->FreeHandlesPendingToken(helper_->InsertToken());
  return true;
}

void WebGPUCommandSerializer::SetClientAwaitingFlush(bool awaiting_flush) {
  // If awaiting_flush is true, but the c2s_buffer_ is invalid (empty), that
  // means the last command right before this caused a flush. Another flush is
  // not needed.
  client_awaiting_flush_ = awaiting_flush && c2s_buffer_.valid();
}

void WebGPUCommandSerializer::HandleGpuControlLostContext() {
  // Immediately forget pending commands.
  c2s_buffer_.Discard();
  c2s_transfer_buffer_ = nullptr;

  // Disconnect the wire client. WebGPU commands will be serialized into dummy
  // space owned by the wire client, and the device will receive a Lost event.
  // No commands will be sent after this point.
  // NOTE: This assumes single-threaded operation.
  // TODO(enga): Implement context reset/recovery.
  wire_client_->Disconnect();
}

WGPUDevice WebGPUCommandSerializer::GetDevice() const {
  return wire_client_->GetDevice();
}

ReservedTexture WebGPUCommandSerializer::ReserveTexture() {
  dawn_wire::ReservedTexture reservation =
      wire_client_->ReserveTexture(GetDevice());
  return {reservation.texture, reservation.id, reservation.generation};
}

bool WebGPUCommandSerializer::HandleCommands(const char* commands,
                                             size_t command_size) {
  return wire_client_->HandleCommands(commands, command_size);
}
#endif

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
  // Wait for all commands to finish or we may free shared memory while
  // commands are still in flight.
  FlushAllCommandSerializers();
#endif

  helper_->Finish();

#if BUILDFLAG(USE_DAWN)
  // Now that commands are finished, free the wire client.
  ClearAllCommandSerializers();

  // All client-side Dawn objects are now destroyed.
  // Shared memory allocations for buffers that were still mapped at the time
  // of destruction can now be safely freed.
  memory_transfer_service_->FreeHandlesPendingToken(helper_->InsertToken());
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

  procs_ = dawn_wire::WireClient::GetProcs();

  // TODO(senorblanco): Do this only once per process. Doing it once per
  // WebGPUImplementation is non-optimal but valid valid, since the returned
  // procs are always the same.
  dawnProcSetProcs(&procs_);
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
  for (auto& iter : command_serializers_) {
    iter.second->HandleGpuControlLostContext();
  }
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
#if BUILDFLAG(USE_DAWN)

  static uint32_t return_trace_id = 0;
  TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                        "DawnReturnCommands", return_trace_id++);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUImplementation::OnGpuControlReturnData", "bytes",
               data.size());

  if (data.size() <= sizeof(cmds::DawnReturnDataHeader)) {
    // TODO(jiawei.shao@intel.com): Lose the context.
    NOTREACHED();
    return;
  }
  const cmds::DawnReturnDataHeader& dawnReturnDataHeader =
      *reinterpret_cast<const cmds::DawnReturnDataHeader*>(data.data());

  switch (dawnReturnDataHeader.return_data_type) {
    case DawnReturnDataType::kDawnCommands: {
      if (data.size() < sizeof(cmds::DawnReturnCommandsInfo)) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }

      const cmds::DawnReturnCommandsInfo* dawn_return_commands_info =
          reinterpret_cast<const cmds::DawnReturnCommandsInfo*>(data.data());
      DawnDeviceClientID device_client_id =
          dawn_return_commands_info->header.device_client_id;
      WebGPUCommandSerializer* command_serializer =
          GetCommandSerializerWithDeviceClientID(device_client_id);
      if (!command_serializer) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }
      if (!command_serializer->HandleCommands(
              reinterpret_cast<const char*>(
                  dawn_return_commands_info->deserialized_buffer),
              data.size() - offsetof(cmds::DawnReturnCommandsInfo,
                                     deserialized_buffer))) {
        // TODO(enga): Lose the context.
        NOTREACHED();
      }
    } break;
    case DawnReturnDataType::kRequestedDawnAdapterProperties: {
      if (data.size() < sizeof(cmds::DawnReturnAdapterInfo)) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }

      const cmds::DawnReturnAdapterInfo* returned_adapter_info =
          reinterpret_cast<const cmds::DawnReturnAdapterInfo*>(data.data());

      DawnRequestAdapterSerial request_adapter_serial =
          returned_adapter_info->header.request_adapter_serial;
      auto request_callback_iter =
          request_adapter_callback_map_.find(request_adapter_serial);
      if (request_callback_iter == request_adapter_callback_map_.end()) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }
      auto& request_callback = request_callback_iter->second;
      GLuint adapter_service_id =
          returned_adapter_info->header.adapter_service_id;
      WGPUDeviceProperties adapter_properties = {};
      const volatile char* deserialized_buffer =
          reinterpret_cast<const volatile char*>(
              returned_adapter_info->deserialized_buffer);
      dawn_wire::DeserializeWGPUDeviceProperties(&adapter_properties,
                                                 deserialized_buffer);
      std::move(request_callback).Run(adapter_service_id, adapter_properties);
      request_adapter_callback_map_.erase(request_callback_iter);
    } break;
    case DawnReturnDataType::kRequestedDeviceReturnInfo: {
      if (data.size() < sizeof(cmds::DawnReturnRequestDeviceInfo)) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }

      const cmds::DawnReturnRequestDeviceInfo* returned_request_device_info =
          reinterpret_cast<const cmds::DawnReturnRequestDeviceInfo*>(
              data.data());

      DawnDeviceClientID device_client_id =
          returned_request_device_info->device_client_id;
      auto request_callback_iter =
          request_device_callback_map_.find(device_client_id);
      if (request_callback_iter == request_device_callback_map_.end()) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }
      auto& request_callback = request_callback_iter->second;
      bool is_request_device_success =
          returned_request_device_info->is_request_device_success;
      if (!is_request_device_success) {
        auto iter = command_serializers_.find(device_client_id);
        DCHECK(iter != command_serializers_.end());
        command_serializers_.erase(iter);
      }
      std::move(request_callback)
          .Run(is_request_device_success, device_client_id);
      request_device_callback_map_.erase(request_callback_iter);
    } break;
    default:
      // TODO(jiawei.shao@intel.com): Lose the context.
      NOTREACHED();
      break;
  }
#endif
}

const DawnProcTable& WebGPUImplementation::GetProcs() const {
#if !BUILDFLAG(USE_DAWN)
  NOTREACHED();
#endif
  return procs_;
}

#if BUILDFLAG(USE_DAWN)
WebGPUCommandSerializer*
WebGPUImplementation::GetCommandSerializerWithDeviceClientID(
    DawnDeviceClientID device_client_id) const {
  auto command_serializer = command_serializers_.find(device_client_id);
  if (command_serializer == command_serializers_.end()) {
    return nullptr;
  }
  return command_serializer->second.get();
}

void WebGPUImplementation::FlushAllCommandSerializers() {
  for (auto& iter : command_serializers_) {
    iter.second->Flush();
  }
}

void WebGPUImplementation::ClearAllCommandSerializers() {
  command_serializers_.clear();
}

bool WebGPUImplementation::AddNewCommandSerializer(
    DawnDeviceClientID device_client_id) {
  std::unique_ptr<TransferBuffer> c2s_transfer_buffer =
      std::make_unique<TransferBuffer>(helper_);
  const SharedMemoryLimits& limits = SharedMemoryLimits::ForWebGPUContext();
  if (!c2s_transfer_buffer->Initialize(
          limits.start_transfer_buffer_size,
          ImplementationBase::kStartingOffset, limits.min_transfer_buffer_size,
          limits.max_transfer_buffer_size, ImplementationBase::kAlignment)) {
    return false;
  }
  command_serializers_[device_client_id] =
      std::make_unique<WebGPUCommandSerializer>(device_client_id, helper_,
                                                memory_transfer_service_.get(),
                                                std::move(c2s_transfer_buffer));
  return true;
}
#endif

void WebGPUImplementation::FlushCommands() {
#if BUILDFLAG(USE_DAWN)
  FlushAllCommandSerializers();
#endif
  helper_->Flush();
}

void WebGPUImplementation::FlushCommands(DawnDeviceClientID device_client_id) {
#if BUILDFLAG(USE_DAWN)
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  command_serializer->Flush();
  helper_->Flush();
#endif
}

void WebGPUImplementation::EnsureAwaitingFlush(
    DawnDeviceClientID device_client_id,
    bool* needs_flush) {
#if BUILDFLAG(USE_DAWN)
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);

  // If there is already a flush waiting, we don't need to flush.
  // We only want to set |needs_flush| on state transition from
  // false -> true.
  if (command_serializer->ClientAwaitingFlush()) {
    *needs_flush = false;
    return;
  }

  // Set the state to waiting for flush, and then write |needs_flush|.
  // Could still be false if there's no data to flush.
  command_serializer->SetClientAwaitingFlush(true);
  *needs_flush = command_serializer->ClientAwaitingFlush();
#else
  *needs_flush = false;
#endif
}

void WebGPUImplementation::FlushAwaitingCommands(
    DawnDeviceClientID device_client_id) {
#if BUILDFLAG(USE_DAWN)
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  if (command_serializer->ClientAwaitingFlush()) {
    command_serializer->Flush();
    helper_->Flush();
  }
#endif
}

WGPUDevice WebGPUImplementation::GetDevice(
    DawnDeviceClientID device_client_id) {
#if BUILDFLAG(USE_DAWN)
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  return command_serializer->GetDevice();
#else
  NOTREACHED();
  return {};
#endif
}

ReservedTexture WebGPUImplementation::ReserveTexture(
    DawnDeviceClientID device_client_id) {
#if BUILDFLAG(USE_DAWN)
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  return command_serializer->ReserveTexture();
#else
  NOTREACHED();
  return {};
#endif
}

DawnRequestAdapterSerial WebGPUImplementation::NextRequestAdapterSerial() {
  return ++request_adapter_serial_;
}

bool WebGPUImplementation::RequestAdapterAsync(
    PowerPreference power_preference,
    base::OnceCallback<void(int32_t, const WGPUDeviceProperties&)>
        request_adapter_callback) {
  if (lost_) {
    return false;
  }

  // Now that we declare request_adapter_serial as an uint64, it can't overflow
  // because we just increment an uint64 by one.
  DawnRequestAdapterSerial request_adapter_serial = NextRequestAdapterSerial();
  DCHECK(request_adapter_callback_map_.find(request_adapter_serial) ==
         request_adapter_callback_map_.end());

  helper_->RequestAdapter(request_adapter_serial,
                          static_cast<uint32_t>(power_preference));
  helper_->Flush();

  request_adapter_callback_map_[request_adapter_serial] =
      std::move(request_adapter_callback);

  return true;
}

DawnDeviceClientID WebGPUImplementation::NextDeviceClientID() {
  return ++device_client_id_;
}

bool WebGPUImplementation::RequestDeviceAsync(
    uint32_t requested_adapter_id,
    const WGPUDeviceProperties& requested_device_properties,
    base::OnceCallback<void(bool, DawnDeviceClientID)>
        request_device_callback) {
#if BUILDFLAG(USE_DAWN)
  if (lost_) {
    return false;
  }

  // Now that we declare device_client_id as an uint64, it can't overflow
  // because we just increment an uint64 by one.
  DawnDeviceClientID device_client_id = NextDeviceClientID();
  DCHECK(request_device_callback_map_.find(device_client_id) ==
         request_device_callback_map_.end());

  DCHECK(command_serializers_.find(device_client_id) ==
         command_serializers_.end());
  if (!AddNewCommandSerializer(device_client_id)) {
    return false;
  }
  request_device_callback_map_[device_client_id] =
      std::move(request_device_callback);

  command_serializers_[device_client_id]->RequestDeviceCreation(
      requested_adapter_id, requested_device_properties);

  return true;
#else
  NOTREACHED();
  return false;
#endif
}

void WebGPUImplementation::AssociateMailbox(GLuint64 device_client_id,
                                            GLuint device_generation,
                                            GLuint id,
                                            GLuint generation,
                                            GLuint usage,
                                            const GLbyte* mailbox) {
#if BUILDFLAG(USE_DAWN)
  // Flush previous Dawn commands as they may manipulate texture object IDs
  // and need to be resolved prior to the AssociateMailbox command. Otherwise
  // the service side might not know, for example that the previous texture
  // using that ID has been released.
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  command_serializer->Flush();

  helper_->AssociateMailboxImmediate(device_client_id, device_generation, id,
                                     generation, usage, mailbox);
#endif
}

void WebGPUImplementation::DissociateMailbox(GLuint64 device_client_id,
                                             GLuint texture_id,
                                             GLuint texture_generation) {
#if BUILDFLAG(USE_DAWN)
  // Flush previous Dawn commands that might be rendering to the texture, prior
  // to Dissociating the shared image from that texture.
  WebGPUCommandSerializer* command_serializer =
      GetCommandSerializerWithDeviceClientID(device_client_id);
  DCHECK(command_serializer);
  command_serializer->Flush();

  helper_->DissociateMailbox(device_client_id, texture_id, texture_generation);
#endif
}

void WebGPUImplementation::RemoveDevice(DawnDeviceClientID device_client_id) {
#if BUILDFLAG(USE_DAWN)
  auto it = command_serializers_.find(device_client_id);
  DCHECK(it != command_serializers_.end());
  helper_->RemoveDevice(device_client_id);
  command_serializers_.erase(it);
#endif
}

}  // namespace webgpu
}  // namespace gpu
