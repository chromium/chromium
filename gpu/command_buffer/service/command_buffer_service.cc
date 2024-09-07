// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_finch_features.h"

#if BUILDFLAG(IS_MAC)
#include <mach/mach_vm.h>
#include <mach/vm_purgable.h>
#include <mach/vm_statistics.h>

#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/process_memory_dump.h"
#endif

namespace gpu {

#if BUILDFLAG(IS_MAC)
namespace {
class AppleGpuMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  AppleGpuMemoryDumpProvider();
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // NoDestructor only.
  ~AppleGpuMemoryDumpProvider() override = default;
};

AppleGpuMemoryDumpProvider::AppleGpuMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "CommandBuffer", nullptr);
}

bool AppleGpuMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // Collect IOSurface total memory usage.
  size_t surface_virtual_size = 0;
  size_t surface_resident_size = 0;
  size_t surface_swapped_out_size = 0;
  size_t surface_dirty_size = 0;
  size_t surface_nonpurgeable_size = 0;
  size_t surface_purgeable_size = 0;

  // And IOAccelerator. Per vm_statistics.h in XNU, this is used to
  // "differentiate memory needed by GPU drivers and frameworks from generic
  // IOKit allocations". See xnu-1456.1.26/osfmk/mach/vm_statistics.h.
  size_t accelerator_virtual_size = 0;
  size_t accelerator_resident_size = 0;
  size_t accelerator_swapped_out_size = 0;
  size_t accelerator_dirty_size = 0;
  size_t accelerator_nonpurgeable_size = 0;
  size_t accelerator_purgeable_size = 0;

  task_t task = mach_task_self();
  mach_vm_address_t address = 0;
  mach_vm_size_t size = 0;

  while (true) {
    address += size;

    // GetBasicInfo is faster than querying the extended attributes. Query this
    // first to filter out regions that cannot correspond to IOSurfaces.
    vm_region_basic_info_64 basic_info;
    base::MachVMRegionResult result =
        base::GetBasicInfo(task, &size, &address, &basic_info);
    if (result == base::MachVMRegionResult::Finished) {
      break;
    } else if (result == base::MachVMRegionResult::Error) {
      return false;
    }

    // All IOSurfaces and IOAccelerator allocations seen locally (M1 laptop)
    // have rw-/rw- permissions. More distinctive characteristics require the
    // extended info, which are more expensive to query.
    const vm_prot_t rw = VM_PROT_READ | VM_PROT_WRITE;
    if (basic_info.protection != rw || basic_info.max_protection != rw)
      continue;

    // Candidate, need the extended info to get the user tag, but also the page
    // status breakdown.
    vm_region_extended_info_data_t info;
    mach_port_t object_name;
    mach_msg_type_number_t count;

    count = VM_REGION_EXTENDED_INFO_COUNT;
    kern_return_t ret = mach_vm_region(
        task, &address, &size, VM_REGION_EXTENDED_INFO,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object_name);
    // No regions above the requested address.
    if (ret == KERN_INVALID_ADDRESS)
      break;

    if (ret != KERN_SUCCESS)
      return false;

    if (info.user_tag != VM_MEMORY_IOSURFACE &&
        info.user_tag != VM_MEMORY_IOACCELERATOR) {
      continue;
    }

    int purgeable_state = 0;
    ret = mach_vm_purgable_control(task, address, VM_PURGABLE_GET_STATE,
                                   &purgeable_state);

    purgeable_state = purgeable_state & VM_PURGABLE_STATE_MASK;

    switch (info.user_tag) {
      case VM_MEMORY_IOSURFACE:
        surface_virtual_size += size;
        surface_resident_size += info.pages_resident * base::GetPageSize();
        surface_swapped_out_size +=
            info.pages_swapped_out * base::GetPageSize();
        surface_dirty_size += info.pages_dirtied * base::GetPageSize();
        if (purgeable_state == VM_PURGABLE_VOLATILE ||
            purgeable_state == VM_PURGABLE_EMPTY) {
          surface_purgeable_size += size;
        } else {
          surface_nonpurgeable_size += size;
        }
        break;
      case VM_MEMORY_IOACCELERATOR:
        accelerator_virtual_size += size;
        accelerator_resident_size += info.pages_resident * base::GetPageSize();
        accelerator_swapped_out_size +=
            info.pages_swapped_out * base::GetPageSize();
        accelerator_dirty_size += info.pages_dirtied * base::GetPageSize();
        if (purgeable_state == VM_PURGABLE_VOLATILE ||
            purgeable_state == VM_PURGABLE_EMPTY) {
          accelerator_purgeable_size += size;
        } else {
          accelerator_nonpurgeable_size += size;
        }
        break;
    }
  }

  auto* dump = pmd->CreateAllocatorDump("iosurface");
  dump->AddScalar("virtual_size", "bytes", surface_virtual_size);
  dump->AddScalar("resident_size", "bytes", surface_resident_size);
  dump->AddScalar("swapped_out_size", "bytes", surface_swapped_out_size);
  dump->AddScalar("dirty_size", "bytes", surface_dirty_size);
  dump->AddScalar("size", "bytes", surface_virtual_size);
  // Some IOSurfaces have a non-trivial difference between their mapped size
  // and their "dirty" size, possibly because some of it has been marked
  // purgeable, and has been purged (rather than swapped out). Report resident
  // + swapped, as it is the fraction of memory which is: (a) using actual
  // memory, and (b) counted in private memory footprint.
  //
  // Note: not using "dirty_size", as it doesn't contain the swapped out part.
  dump->AddScalar("resident_swapped", "bytes",
                  surface_resident_size + surface_swapped_out_size);
  dump->AddScalar("nonpurgeable_size", "bytes", surface_nonpurgeable_size);
  dump->AddScalar("purgeable_size", "bytes", surface_purgeable_size);

  // Ditto for IOAccelerator.
  dump = pmd->CreateAllocatorDump("ioaccelerator");
  dump->AddScalar("virtual_size", "bytes", accelerator_virtual_size);
  dump->AddScalar("resident_size", "bytes", accelerator_resident_size);
  dump->AddScalar("swapped_out_size", "bytes", accelerator_swapped_out_size);
  dump->AddScalar("dirty_size", "bytes", accelerator_dirty_size);
  dump->AddScalar("size", "bytes", accelerator_virtual_size);
  dump->AddScalar("resident_swapped", "bytes",
                  accelerator_resident_size + accelerator_swapped_out_size);
  dump->AddScalar("nonpurgeable_size", "bytes", accelerator_nonpurgeable_size);
  dump->AddScalar("purgeable_size", "bytes", accelerator_purgeable_size);

  return true;
}
}  // namespace
#endif

// Context switching leads to a render pass break in ANGLE/Vulkan. The command
// buffer has a 20-command limit before it forces a context switch. This
// experiment tests a 100-command limit.
int GetCommandBufferSliceSize() {
  static int slice_size =
      (base::FeatureList::IsEnabled(features::kIncreasedCmdBufferParseSlice)
           ? CommandBufferService::kParseCommandsSliceLarge
           : CommandBufferService::kParseCommandsSliceSmall);
  return slice_size;
}

CommandBufferService::CommandBufferService(CommandBufferServiceClient* client,
                                           MemoryTracker* memory_tracker)
    : client_(client),
      transfer_buffer_manager_(
          std::make_unique<TransferBufferManager>(memory_tracker)) {
  DCHECK(client_);
  state_.token = 0;
#if BUILDFLAG(IS_MAC)
  static base::NoDestructor<AppleGpuMemoryDumpProvider> dump_provider;
#endif
}

CommandBufferService::~CommandBufferService() = default;

void CommandBufferService::UpdateState() {
  ++state_.generation;
  if (shared_state_)
    shared_state_->Write(state_);
}

void CommandBufferService::Flush(int32_t put_offset,
                                 AsyncAPIInterface* handler) {
  DCHECK(handler);
  if (put_offset < 0 || put_offset >= num_entries_) {
    SetParseError(gpu::error::kOutOfBounds);
    return;
  }

  TRACE_EVENT1("gpu", "CommandBufferService:PutChanged", "handler",
               std::string(handler->GetLogPrefix()));

  put_offset_ = put_offset;

  DCHECK(buffer_);

  if (state_.error != error::kNoError)
    return;

  DCHECK(scheduled());

  if (paused_) {
    paused_ = false;
    TRACE_COUNTER_ID1("gpu", "CommandBufferService::Paused", this, paused_);
  }

  handler->BeginDecoding();

  // BeginDecoding can cause context loss due to resuming shared image access.
  if (state_.error != error::kNoError) {
    handler->EndDecoding();
    return;
  }

  int end = put_offset_ < state_.get_offset ? num_entries_ : put_offset_;
  while (put_offset_ != state_.get_offset) {
    int num_entries = end - state_.get_offset;
    int entries_processed = 0;
    error::Error error = handler->DoCommands(GetCommandBufferSliceSize(),
                                             buffer_ + state_.get_offset,
                                             num_entries, &entries_processed);

    state_.get_offset += entries_processed;
    DCHECK_LE(state_.get_offset, num_entries_);
    if (state_.get_offset == num_entries_) {
      end = put_offset_;
      state_.get_offset = 0;
    }

    if (error::IsError(error)) {
      SetParseError(error);
      break;
    }

    if (client_->OnCommandBatchProcessed() ==
        CommandBufferServiceClient::kPauseExecution) {
      paused_ = true;
      TRACE_COUNTER_ID1("gpu", "CommandBufferService::Paused", this, paused_);
      break;
    }

    if (!scheduled())
      break;
  }

  handler->EndDecoding();
}

void CommandBufferService::SetGetBuffer(int32_t transfer_buffer_id) {
  DCHECK((put_offset_ == state_.get_offset) ||
         (state_.error != error::kNoError));
  put_offset_ = 0;
  state_.get_offset = 0;
  ++state_.set_get_buffer_count;

  // If the buffer is invalid we handle it gracefully.
  // This means `transfer_buffer` can be nullptr.
  auto transfer_buffer = GetTransferBuffer(transfer_buffer_id);
  if (transfer_buffer) {
    uint32_t size = transfer_buffer->size();
    volatile void* memory = transfer_buffer->memory();
    // check proper alignments.
    DCHECK_EQ(
        0u, (reinterpret_cast<intptr_t>(memory)) % alignof(CommandBufferEntry));
    DCHECK_EQ(0u, size % sizeof(CommandBufferEntry));

    num_entries_ = size / sizeof(CommandBufferEntry);
    buffer_ = reinterpret_cast<volatile CommandBufferEntry*>(memory);
  } else {
    num_entries_ = 0;
    buffer_ = nullptr;
  }
  ring_buffer_ = std::move(transfer_buffer);
  UpdateState();
}

void CommandBufferService::SetSharedStateBuffer(
    std::unique_ptr<BufferBacking> shared_state_buffer) {
  shared_state_buffer_ = std::move(shared_state_buffer);
  DCHECK(shared_state_buffer_->GetSize() >= sizeof(*shared_state_));

  shared_state_ =
      static_cast<CommandBufferSharedState*>(shared_state_buffer_->GetMemory());

  UpdateState();
}

CommandBuffer::State CommandBufferService::GetState() {
  return state_;
}

void CommandBufferService::SetReleaseCount(uint64_t release_count) {
  DLOG_IF(ERROR, release_count < state_.release_count)
      << "Non-monotonic SetReleaseCount";
  state_.release_count = release_count;
  UpdateState();
}

scoped_refptr<Buffer> CommandBufferService::CreateTransferBuffer(
    uint32_t size,
    int32_t* id,
    uint32_t alignment) {
  *id = GetNextBufferId();
  auto result = CreateTransferBufferWithId(size, *id, alignment);
  if (!result) {
    *id = -1;
  }
  return result;
}

void CommandBufferService::DestroyTransferBuffer(int32_t id) {
  transfer_buffer_manager_->DestroyTransferBuffer(id);
}

scoped_refptr<Buffer> CommandBufferService::GetTransferBuffer(int32_t id) {
  return transfer_buffer_manager_->GetTransferBuffer(id);
}

bool CommandBufferService::RegisterTransferBuffer(
    int32_t id,
    scoped_refptr<Buffer> buffer) {
  return transfer_buffer_manager_->RegisterTransferBuffer(id,
                                                          std::move(buffer));
}

scoped_refptr<Buffer> CommandBufferService::CreateTransferBufferWithId(
    uint32_t size,
    int32_t id,
    uint32_t alignment) {
  scoped_refptr<Buffer> buffer = MakeMemoryBuffer(size, alignment);
  if (!RegisterTransferBuffer(id, buffer)) {
    SetParseError(gpu::error::kOutOfBounds);
    return nullptr;
  }

  return buffer;
}

void CommandBufferService::SetToken(int32_t token) {
  state_.token = token;
  UpdateState();
}

void CommandBufferService::SetParseError(error::Error error) {
  if (state_.error == error::kNoError) {
    state_.error = error;
    client_->OnParseError();
  }
}

void CommandBufferService::SetContextLostReason(
    error::ContextLostReason reason) {
  state_.context_lost_reason = reason;
}

bool CommandBufferService::ShouldYield() {
  return client_->OnCommandBatchProcessed() ==
         CommandBufferServiceClient::kPauseExecution;
}

void CommandBufferService::SetScheduled(bool scheduled) {
  TRACE_EVENT2("gpu", "CommandBufferService:SetScheduled", "this",
               static_cast<void*>(this), "scheduled", scheduled);
  scheduled_ = scheduled;
}

size_t CommandBufferService::GetSharedMemoryBytesAllocated() const {
  return transfer_buffer_manager_->shared_memory_bytes_allocated();
}

}  // namespace gpu
