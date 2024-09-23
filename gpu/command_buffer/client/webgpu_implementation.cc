// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <dawn/wire/client/webgpu.h>

#include <algorithm>
#include <vector>

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

#if BUILDFLAG(USE_DAWN)
DawnWireServices::~DawnWireServices() {
  wgpuDawnWireClientInstanceRelease(wgpu_instance_);
}

DawnWireServices::DawnWireServices(
    WebGPUImplementation* webgpu_implementation,
    WebGPUCmdHelper* helper,
    MappedMemoryManager* mapped_memory,
    std::unique_ptr<TransferBuffer> transfer_buffer)
    : memory_transfer_service_(mapped_memory),
      serializer_(webgpu_implementation,
                  helper,
                  &memory_transfer_service_,
                  std::move(transfer_buffer)),
      wire_client_(dawn::wire::WireClientDescriptor{
          &serializer_,
          &memory_transfer_service_,
      }),
      wgpu_instance_(wire_client_.ReserveInstance().instance) {
  DCHECK(wgpu_instance_);
}

WGPUInstance DawnWireServices::GetWGPUInstance() const {
  return wgpu_instance_;
}

dawn::wire::WireClient* DawnWireServices::wire_client() {
  return &wire_client_;
}
DawnClientSerializer* DawnWireServices::serializer() {
  return &serializer_;
}
DawnClientMemoryTransferService* DawnWireServices::memory_transfer_service() {
  return &memory_transfer_service_;
}

void DawnWireServices::Disconnect() {
  disconnected_ = true;
  wire_client_.Disconnect();
  serializer_.Disconnect();
  memory_transfer_service_.Disconnect();
}

bool DawnWireServices::IsDisconnected() const {
  return disconnected_;
}

void DawnWireServices::FreeMappedResources(WebGPUCmdHelper* helper) {
  memory_transfer_service_.FreeHandles(helper);
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
  LoseContext();

  // Before destroying WebGPUImplementation, all mappable buffers
  // must be destroyed first. This means that all shared memory mappings are
  // detached. If they are not destroyed, MappedMemoryManager (member of
  // base class ImplementationBase) will assert on destruction that some
  // memory blocks are in use. Calling |FreeMappedResources| marks all
  // blocks that are no longer in use as free.
#if BUILDFLAG(USE_DAWN)
  if (dawn_wire_) {
    dawn_wire_->FreeMappedResources(helper_);
  }
#endif

  // Wait for commands to finish before we continue destruction.
  // WebGPUImplementation no longer owns the WebGPU transfer buffer, but still
  // owns the GPU command buffer. We should not free shared memory that the
  // GPU process is using.
  helper_->Finish();
}

void WebGPUImplementation::LoseContext() {
  lost_ = true;
#if BUILDFLAG(USE_DAWN)
  if (dawn_wire_) {
    dawn_wire_->Disconnect();
  }
#endif
}

gpu::ContextResult WebGPUImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "WebGPUImplementation::Initialize");
  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

  std::unique_ptr<TransferBuffer> transfer_buffer =
      std::make_unique<TransferBuffer>(helper_);
  if (!transfer_buffer->Initialize(
          limits.start_transfer_buffer_size,
          /* start offset */ 0, limits.min_transfer_buffer_size,
          limits.max_transfer_buffer_size, kAlignment)) {
    return gpu::ContextResult::kFatalFailure;
  }

#if BUILDFLAG(USE_DAWN)
  dawn_wire_ = base::MakeRefCounted<DawnWireServices>(
      this, helper_, mapped_memory_.get(), std::move(transfer_buffer));
#endif

  return gpu::ContextResult::kSuccess;
}

// ContextSupport implementation.
void WebGPUImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  NOTIMPLEMENTED();
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
  NOTREACHED_IN_MIGRATION();
  return false;
}
void WebGPUImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED_IN_MIGRATION();
}
bool WebGPUImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED_IN_MIGRATION();
  return false;
}
void* WebGPUImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
void WebGPUImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED_IN_MIGRATION();
}
bool WebGPUImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED_IN_MIGRATION();
  return false;
}
void WebGPUImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED_IN_MIGRATION();
}
void WebGPUImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  NOTREACHED_IN_MIGRATION();
}
unsigned int WebGPUImplementation::GetTransferBufferFreeSize() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}
bool WebGPUImplementation::IsJpegDecodeAccelerationSupported() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}
bool WebGPUImplementation::IsWebPDecodeAccelerationSupported() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}
bool WebGPUImplementation::CanDecodeWithHardwareAcceleration(
    const cc::ImageHeaderMetadata* image_metadata) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

// InterfaceBase implementation.
void WebGPUImplementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  // Need to commit the commands to the GPU command buffer first for SyncToken
  // to work.
#if BUILDFLAG(USE_DAWN)
  dawn_wire_->serializer()->Commit();
#endif
  ImplementationBase::GenSyncToken(sync_token);
}
void WebGPUImplementation::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  // Need to commit the commands to the GPU command buffer first for SyncToken
  // to work.
#if BUILDFLAG(USE_DAWN)
  dawn_wire_->serializer()->Commit();
#endif
  ImplementationBase::GenUnverifiedSyncToken(sync_token);
}
void WebGPUImplementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                    GLsizei count) {
  ImplementationBase::VerifySyncTokens(sync_tokens, count);
}
void WebGPUImplementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  // Need to commit the commands to the GPU command buffer first for SyncToken
  // to work.
#if BUILDFLAG(USE_DAWN)
  dawn_wire_->serializer()->Commit();
#endif
  ImplementationBase::WaitSyncToken(sync_token);
}
void WebGPUImplementation::ShallowFlushCHROMIUM() {
  FlushCommands();
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
void WebGPUImplementation::OnGpuControlLostContext() {
  LoseContext();

  // This should never occur more than once.
  DCHECK(!lost_context_callback_run_);
  lost_context_callback_run_ = true;
  if (!lost_context_callback_.is_null()) {
    std::move(lost_context_callback_).Run();
  }
}
void WebGPUImplementation::OnGpuControlLostContextMaybeReentrant() {
  // If this function is called, we are guaranteed to also get a call
  // to |OnGpuControlLostContext| when the callstack unwinds. Thus, this
  // function only handles immediately setting state so that other operations
  // which occur while the callstack is unwinding are aware that the context
  // is lost.
  lost_ = true;
}
void WebGPUImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  if (lost_) {
    return;
  }
#if BUILDFLAG(USE_DAWN)

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
      if (dawn_wire_->IsDisconnected()) {
        break;
      }

      TRACE_EVENT_WITH_FLOW0(
          TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnReturnCommands",
          dawn_return_commands_info->header.return_data_header.trace_id,
          TRACE_EVENT_FLAG_FLOW_IN);

      // TODO(enga): Instead of a CHECK, this could generate a device lost
      // event on just that device. It doesn't seem worth doing right now
      // since a failure here is likely not recoverable.
      CHECK(dawn_wire_->wire_client()->HandleCommands(
          reinterpret_cast<const char*>(
              dawn_return_commands_info->deserialized_buffer),
          data.size() -
              offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer)));
    } break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
#endif
}

void WebGPUImplementation::FlushCommands() {
#if BUILDFLAG(USE_DAWN)
  dawn_wire_->serializer()->Commit();
  helper_->Flush();
#endif
}

bool WebGPUImplementation::EnsureAwaitingFlush() {
#if BUILDFLAG(USE_DAWN)
  // If there is already a flush waiting, we don't need to flush.
  // We only want to ask for a flush on state transition from
  // false -> true.
  if (dawn_wire_->serializer()->AwaitingFlush()) {
    return false;
  }

  // Set the state to waiting for flush.
  dawn_wire_->serializer()->SetAwaitingFlush(true);
  return true;
#else
  return false;
#endif
}

void WebGPUImplementation::FlushAwaitingCommands() {
#if BUILDFLAG(USE_DAWN)
  dawn_wire_->serializer()->Commit();
  helper_->FlushLazy();
  dawn_wire_->serializer()->SetAwaitingFlush(false);
#endif
}

scoped_refptr<APIChannel> WebGPUImplementation::GetAPIChannel() const {
#if BUILDFLAG(USE_DAWN)
  return dawn_wire_.get();
#else
  return nullptr;
#endif
}

ReservedTexture WebGPUImplementation::ReserveTexture(
    WGPUDevice device,
    const WGPUTextureDescriptor* optionalDesc) {
#if BUILDFLAG(USE_DAWN)
  // Commit because we need to make sure messages that free a previously used
  // texture are seen first. ReserveTexture may reuse an existing ID.
  dawn_wire_->serializer()->Commit();

  WGPUTextureDescriptor placeholderDesc;
  if (optionalDesc == nullptr) {
    placeholderDesc = {};  // Zero initialize.
    optionalDesc = &placeholderDesc;
  }

  auto reserved =
      dawn_wire_->wire_client()->ReserveTexture(device, optionalDesc);
  ReservedTexture result;
  result.texture = reserved.texture;
  result.id = reserved.handle.id;
  result.generation = reserved.handle.generation;
  result.deviceId = reserved.deviceHandle.id;
  result.deviceGeneration = reserved.deviceHandle.generation;
  return result;
#else
  NOTREACHED_IN_MIGRATION();
  return {};
#endif
}

WGPUDevice WebGPUImplementation::DeprecatedEnsureDefaultDeviceSync() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebGPUImplementation::AssociateMailbox(
    GLuint device_id,
    GLuint device_generation,
    GLuint texture_id,
    GLuint texture_generation,
    uint64_t usage,
    uint64_t internal_usage,
    const WGPUTextureFormat* view_formats,
    GLuint view_format_count,
    MailboxFlags flags,
    const Mailbox& mailbox) {
#if BUILDFLAG(USE_DAWN)
  // Commit previous Dawn commands as they may manipulate texture object IDs
  // and need to be resolved prior to the AssociateMailbox command. Otherwise
  // the service side might not know, for example that the previous texture
  // using that ID has been released.
  dawn_wire_->serializer()->Commit();

  // The command buffer transfer data in 4-byte "entries". So the array of data
  // we pass must have a byte-length that's a multiple of 4.
  constexpr size_t kEntrySize = 4u;
  static_assert(sizeof(mailbox.name) % kEntrySize == 0u);
  static_assert(sizeof(WGPUTextureFormat) % kEntrySize == 0u);

  size_t num_bytes =
      sizeof(mailbox.name) + sizeof(WGPUTextureFormat) * view_format_count;
  std::vector<char> immediate_data(num_bytes);

  uint32_t num_entries = ComputeNumEntries(immediate_data.size());

  memcpy(immediate_data.data(), mailbox.name, sizeof(mailbox.name));
  memcpy(immediate_data.data() + sizeof(mailbox.name), view_formats,
         sizeof(WGPUTextureFormat) * view_format_count);

  helper_->AssociateMailboxImmediate(
      device_id, device_generation, texture_id, texture_generation, usage,
      internal_usage, flags, view_format_count, num_entries,
      reinterpret_cast<GLuint*>(immediate_data.data()));
#endif
}

void WebGPUImplementation::DissociateMailbox(GLuint texture_id,
                                             GLuint texture_generation) {
#if BUILDFLAG(USE_DAWN)
  // Commit previous Dawn commands that might be rendering to the texture, prior
  // to Dissociating the shared image from that texture.
  dawn_wire_->serializer()->Commit();
  helper_->DissociateMailbox(texture_id, texture_generation);
#endif
}

void WebGPUImplementation::DissociateMailboxForPresent(
    GLuint device_id,
    GLuint device_generation,
    GLuint texture_id,
    GLuint texture_generation) {
#if BUILDFLAG(USE_DAWN)
  // Commit previous Dawn commands that might be rendering to the texture, prior
  // to Dissociating the shared image from that texture.
  dawn_wire_->serializer()->Commit();
  helper_->DissociateMailboxForPresent(device_id, device_generation, texture_id,
                                       texture_generation);
#endif
}

void WebGPUImplementation::SetWebGPUExecutionContextToken(uint32_t type,
                                                          uint32_t high_high,
                                                          uint32_t high_low,
                                                          uint32_t low_high,
                                                          uint32_t low_low) {
#if BUILDFLAG(USE_DAWN)
  helper_->SetWebGPUExecutionContextToken(type, high_high, high_low, low_high,
                                          low_low);
#endif
}

}  // namespace webgpu
}  // namespace gpu
