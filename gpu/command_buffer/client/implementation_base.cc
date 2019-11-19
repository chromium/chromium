// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/implementation_base.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gpu {

#if !defined(_MSC_VER)
const uint32_t ImplementationBase::kMaxSizeOfSimpleResult;
const uint32_t ImplementationBase::kStartingOffset;
#endif

ImplementationBase::ImplementationBase(CommandBufferHelper* helper,
                                       TransferBufferInterface* transfer_buffer,
                                       GpuControl* gpu_control)
    : transfer_buffer_(transfer_buffer),
      gpu_control_(gpu_control),
      capabilities_(gpu_control->GetCapabilities()),
      helper_(helper) {}

ImplementationBase::~ImplementationBase() {
  // The gpu_control_ outlives this class, so clear the client on it before we
  // self-destruct.
  gpu_control_->SetGpuControlClient(nullptr);
}

void ImplementationBase::FreeUnusedSharedMemory() {
  mapped_memory_->FreeUnused();
}

void ImplementationBase::FreeEverything() {
  query_tracker_->Shrink(helper_);
  FreeUnusedSharedMemory();
  transfer_buffer_->Free();
  helper_->FreeRingBuffer();
}

void ImplementationBase::SetLostContextCallback(base::OnceClosure callback) {
  lost_context_callback_ = std::move(callback);
}

void ImplementationBase::FlushPendingWork() {
  gpu_control_->FlushPendingWork();
}

void ImplementationBase::SignalSyncToken(const SyncToken& sync_token,
                                         base::OnceClosure callback) {
  SyncToken verified_sync_token;
  if (sync_token.HasData() &&
      GetVerifiedSyncTokenForIPC(sync_token, &verified_sync_token)) {
    // We can only send verified sync tokens across IPC.
    gpu_control_->SignalSyncToken(
        verified_sync_token,
        base::BindOnce(&ImplementationBase::RunIfContextNotLost,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    // Invalid sync token, just call the callback immediately.
    std::move(callback).Run();
  }
}

// This may be called from any thread. It's safe to access gpu_control_ without
// the lock because it is const.
bool ImplementationBase::IsSyncTokenSignaled(const SyncToken& sync_token) {
  // Check that the sync token belongs to this context.
  DCHECK_EQ(gpu_control_->GetNamespaceID(), sync_token.namespace_id());
  DCHECK_EQ(gpu_control_->GetCommandBufferID(), sync_token.command_buffer_id());
  return gpu_control_->IsFenceSyncReleased(sync_token.release_count());
}

void ImplementationBase::GenSyncToken(GLbyte* sync_token) {
  if (!sync_token) {
    SetGLError(GL_INVALID_VALUE, "glGenSyncTokenCHROMIUM", "empty sync_token");
    return;
  }

  uint64_t fence_sync = gpu_control_->GenerateFenceSyncRelease();
  helper_->InsertFenceSync(fence_sync);
  helper_->CommandBufferHelper::OrderingBarrier();
  gpu_control_->EnsureWorkVisible();

  // Copy the data over after setting the data to ensure alignment.
  SyncToken sync_token_data(gpu_control_->GetNamespaceID(),
                            gpu_control_->GetCommandBufferID(), fence_sync);
  sync_token_data.SetVerifyFlush();
  memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
}

void ImplementationBase::GenUnverifiedSyncToken(GLbyte* sync_token) {
  if (!sync_token) {
    SetGLError(GL_INVALID_VALUE, "glGenUnverifiedSyncTokenCHROMIUM",
               "empty sync_token");
    return;
  }

  uint64_t fence_sync = gpu_control_->GenerateFenceSyncRelease();
  helper_->InsertFenceSync(fence_sync);
  helper_->CommandBufferHelper::OrderingBarrier();

  // Copy the data over after setting the data to ensure alignment.
  SyncToken sync_token_data(gpu_control_->GetNamespaceID(),
                            gpu_control_->GetCommandBufferID(), fence_sync);
  memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
}

void ImplementationBase::VerifySyncTokens(GLbyte** sync_tokens, GLsizei count) {
  bool requires_synchronization = false;
  for (GLsizei i = 0; i < count; ++i) {
    if (sync_tokens[i]) {
      SyncToken sync_token;
      memcpy(&sync_token, sync_tokens[i], sizeof(sync_token));

      if (sync_token.HasData() && !sync_token.verified_flush()) {
        if (!GetVerifiedSyncTokenForIPC(sync_token, &sync_token)) {
          SetGLError(GL_INVALID_VALUE, "glVerifySyncTokensCHROMIUM",
                     "Cannot verify sync token using this context.");
          return;
        }
        requires_synchronization = true;
        DCHECK(sync_token.verified_flush());
      }

      // Set verify bit on empty sync tokens too.
      sync_token.SetVerifyFlush();

      memcpy(sync_tokens[i], &sync_token, sizeof(sync_token));
    }
  }

  // Ensure all the fence syncs are visible on GPU service.
  if (requires_synchronization)
    gpu_control_->EnsureWorkVisible();
}

void ImplementationBase::WaitSyncToken(const GLbyte* sync_token_data) {
  if (!sync_token_data)
    return;

  // Copy the data over before data access to ensure alignment.
  SyncToken sync_token, verified_sync_token;
  memcpy(&sync_token, sync_token_data, sizeof(SyncToken));

  if (!sync_token.HasData())
    return;

  if (!GetVerifiedSyncTokenForIPC(sync_token, &verified_sync_token)) {
    SetGLError(GL_INVALID_VALUE, "glWaitSyncTokenCHROMIUM",
               "Cannot wait on sync_token which has not been verified");
    return;
  }

  // Enqueue sync token in flush after inserting command so that it's not
  // included in an automatic flush.
  gpu_control_->WaitSyncToken(verified_sync_token);
}

void ImplementationBase::SignalQuery(uint32_t query,
                                     base::OnceClosure callback) {
  // Flush previously entered commands to ensure ordering with any
  // glBeginQueryEXT() calls that may have been put into the context.
  IssueShallowFlush();
  gpu_control_->SignalQuery(
      query,
      base::BindOnce(&ImplementationBase::RunIfContextNotLost,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImplementationBase::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  // This ShallowFlush is required to ensure that the GetGpuFence
  // call is processed after the preceding CreateGpuFenceCHROMIUM call.
  IssueShallowFlush();
  gpu_control_->GetGpuFence(gpu_fence_id, std::move(callback));
}

bool ImplementationBase::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  // Dump owned MappedMemoryManager memory as well.
  mapped_memory_->OnMemoryDump(args, pmd);

  if (!transfer_buffer_->HaveBuffer())
    return true;

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(base::StringPrintf(
      "gpu/transfer_buffer_memory/buffer_%d", transfer_buffer_->GetShmId()));
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes,
                  transfer_buffer_->GetSize());

  if (args.level_of_detail != MemoryDumpLevelOfDetail::BACKGROUND) {
    dump->AddScalar("free_size", MemoryAllocatorDump::kUnitsBytes,
                    transfer_buffer_->GetFragmentedFreeSize());
    auto shared_memory_guid = transfer_buffer_->shared_memory_guid();
    const int kImportance = 2;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           kImportance);
    } else {
      auto guid = GetBufferGUIDForTracing(tracing_process_id,
                                          transfer_buffer_->GetShmId());
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

gpu::ContextResult ImplementationBase::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "ImplementationBase::Initialize");
  DCHECK_GE(limits.start_transfer_buffer_size, limits.min_transfer_buffer_size);
  DCHECK_LE(limits.start_transfer_buffer_size, limits.max_transfer_buffer_size);
  DCHECK_GE(limits.min_transfer_buffer_size, kStartingOffset);

  gpu_control_->SetGpuControlClient(this);

  if (!transfer_buffer_->Initialize(
          limits.start_transfer_buffer_size, kStartingOffset,
          limits.min_transfer_buffer_size, limits.max_transfer_buffer_size,
          kAlignment)) {
    // TransferBuffer::Initialize doesn't fail for transient reasons such as if
    // the context was lost. See http://crrev.com/c/720269
    LOG(ERROR) << "ContextResult::kFatalFailure: "
               << "TransferBuffer::Initialize() failed";
    return gpu::ContextResult::kFatalFailure;
  }

  mapped_memory_ = std::make_unique<MappedMemoryManager>(
      helper_, limits.mapped_memory_reclaim_limit);
  mapped_memory_->set_chunk_size_multiple(limits.mapped_memory_chunk_size);
  query_tracker_ = std::make_unique<gles2::QueryTracker>(mapped_memory_.get());

  return gpu::ContextResult::kSuccess;
}

void ImplementationBase::WaitForCmd() {
  TRACE_EVENT0("gpu", "ImplementationBase::WaitForCmd");
  helper_->Finish();
}

int32_t ImplementationBase::GetResultShmId() {
  return transfer_buffer_->GetShmId();
}

bool ImplementationBase::GetBucketContents(uint32_t bucket_id,
                                           std::vector<int8_t>* data) {
  TRACE_EVENT0("gpu", "ImplementationBase::GetBucketContents");
  DCHECK(data);
  const uint32_t kStartSize = 32 * 1024;
  ScopedTransferBufferPtr buffer(kStartSize, helper_, transfer_buffer_);
  if (!buffer.valid()) {
    return false;
  }
  uint32_t size = 0;
  {
    // The Result pointer must be scoped to this block because it can be
    // invalidated below if resizing the ScopedTransferBufferPtr causes the
    // transfer buffer to be reallocated.
    typedef cmd::GetBucketStart::Result Result;
    auto result = GetResultAs<Result>();
    if (!result) {
      return false;
    }
    *result = 0;
    helper_->GetBucketStart(bucket_id, GetResultShmId(), result.offset(),
                            buffer.size(), buffer.shm_id(), buffer.offset());
    WaitForCmd();
    size = *result;
  }
  data->resize(size);
  if (size > 0u) {
    uint32_t offset = 0;
    while (size) {
      if (!buffer.valid()) {
        buffer.Reset(size);
        if (!buffer.valid()) {
          return false;
        }
        helper_->GetBucketData(bucket_id, offset, buffer.size(),
                               buffer.shm_id(), buffer.offset());
        WaitForCmd();
      }
      uint32_t size_to_copy = std::min(size, buffer.size());
      memcpy(&(*data)[offset], buffer.address(), size_to_copy);
      offset += size_to_copy;
      size -= size_to_copy;
      buffer.Release();
    }
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(bucket_id, 0);
  }
  return true;
}

void ImplementationBase::SetBucketContents(uint32_t bucket_id,
                                           const void* data,
                                           uint32_t size) {
  DCHECK(data);
  helper_->SetBucketSize(bucket_id, size);
  if (size > 0u) {
    uint32_t offset = 0;
    while (size) {
      ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
      if (!buffer.valid()) {
        return;
      }
      memcpy(buffer.address(), static_cast<const int8_t*>(data) + offset,
             buffer.size());
      helper_->SetBucketData(bucket_id, offset, buffer.size(), buffer.shm_id(),
                             buffer.offset());
      offset += buffer.size();
      size -= buffer.size();
    }
  }
}

void ImplementationBase::SetBucketAsCString(uint32_t bucket_id,
                                            const char* str) {
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  if (str) {
    base::CheckedNumeric<uint32_t> len = strlen(str);
    len += 1;
    SetBucketContents(bucket_id, str, len.ValueOrDefault(0));
  } else {
    helper_->SetBucketSize(bucket_id, 0);
  }
}

bool ImplementationBase::GetBucketAsString(uint32_t bucket_id,
                                           std::string* str) {
  DCHECK(str);
  std::vector<int8_t> data;
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  if (!GetBucketContents(bucket_id, &data)) {
    return false;
  }
  if (data.empty()) {
    return false;
  }
  str->assign(&data[0], &data[0] + data.size() - 1);
  return true;
}

void ImplementationBase::SetBucketAsString(uint32_t bucket_id,
                                           const std::string& str) {
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  base::CheckedNumeric<uint32_t> len = str.size();
  len += 1;
  SetBucketContents(bucket_id, str.c_str(), len.ValueOrDefault(0));
}

bool ImplementationBase::GetVerifiedSyncTokenForIPC(
    const SyncToken& sync_token,
    SyncToken* verified_sync_token) {
  DCHECK(sync_token.HasData());
  DCHECK(verified_sync_token);

  if (!sync_token.verified_flush() &&
      !gpu_control_->CanWaitUnverifiedSyncToken(sync_token))
    return false;

  *verified_sync_token = sync_token;
  verified_sync_token->SetVerifyFlush();
  return true;
}

void ImplementationBase::RunIfContextNotLost(base::OnceClosure callback) {
  if (!lost_context_callback_run_) {
    std::move(callback).Run();
  }
}

void ImplementationBase::SetGrContext(GrContext* gr) {}

bool ImplementationBase::HasGrContextSupport() const {
  return false;
}

void ImplementationBase::WillCallGLFromSkia() {
  // Should only be called on subclasses that have GrContextSupport
  NOTREACHED();
}

void ImplementationBase::DidCallGLFromSkia() {
  // Should only be called on subclasses that have GrContextSupport
  NOTREACHED();
}

void ImplementationBase::SetDisplayTransform(gfx::OverlayTransform transform) {
  helper_->Flush();
  gpu_control_->SetDisplayTransform(transform);
}

}  // namespace gpu
