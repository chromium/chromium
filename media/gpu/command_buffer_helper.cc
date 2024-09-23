// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/command_buffer_helper.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace media {

class CommandBufferHelperImpl
    : public CommandBufferHelper,
      public gpu::CommandBufferStub::DestructionObserver {
 public:
  explicit CommandBufferHelperImpl(gpu::CommandBufferStub* stub)
      : CommandBufferHelper(stub->channel()->task_runner()),
        stub_(stub),
        memory_tracker_(this),
        memory_type_tracker_(&memory_tracker_) {
    DVLOG(1) << __func__;
    DCHECK(stub_->channel()->task_runner()->BelongsToCurrentThread());

    stub_->AddDestructionObserver(this);
    wait_sequence_id_ = stub_->channel()->scheduler()->CreateSequence(
        gpu::SchedulingPriority::kNormal, stub_->channel()->task_runner());
  }

  CommandBufferHelperImpl(const CommandBufferHelperImpl&) = delete;
  CommandBufferHelperImpl& operator=(const CommandBufferHelperImpl&) = delete;

  void WaitForSyncToken(gpu::SyncToken sync_token,
                        base::OnceClosure done_cb) override {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!stub_) {
      return;
    }

    // TODO(sandersd): Do we need to keep a ref to |this| while there are
    // pending waits? If we destruct while they are pending, they will never
    // run.
    stub_->channel()->scheduler()->ScheduleTask(
        gpu::Scheduler::Task(wait_sequence_id_, std::move(done_cb),
                             std::vector<gpu::SyncToken>({sync_token})));
  }

  // Const variant of GetSharedImageStub() for internal callers.
  gpu::SharedImageStub* shared_image_stub() const {
    if (!stub_) {
      return nullptr;
    }
    return stub_->channel()->shared_image_stub();
  }

#if !BUILDFLAG(IS_ANDROID)
  gpu::SharedImageStub* GetSharedImageStub() override {
    return shared_image_stub();
  }

  gpu::MemoryTypeTracker* GetMemoryTypeTracker() override {
    return &memory_type_tracker_;
  }

  gpu::SharedImageManager* GetSharedImageManager() override {
    if (!stub_) {
      return nullptr;
    }
    return stub_->channel()->gpu_channel_manager()->shared_image_manager();
  }

 public:
#endif

 private:
  // Helper class to forward memory tracking calls to shared image stub.
  // Necessary because the underlying stub and channel can get destroyed before
  // the CommandBufferHelper and its clients.
  class MemoryTrackerImpl : public gpu::MemoryTracker {
   public:
    explicit MemoryTrackerImpl(CommandBufferHelperImpl* helper)
        : helper_(helper) {
      if (auto* stub = helper_->shared_image_stub()) {
        // We assume these don't change after initialization.
        client_id_ = stub->ClientId();
        client_tracing_id_ = stub->ClientTracingId();
        context_group_tracing_id_ = stub->ContextGroupTracingId();
      }
    }
    ~MemoryTrackerImpl() override = default;

    MemoryTrackerImpl(const MemoryTrackerImpl&) = delete;
    MemoryTrackerImpl& operator=(const MemoryTrackerImpl&) = delete;

    void TrackMemoryAllocatedChange(int64_t delta) override {
      if (auto* stub = helper_->shared_image_stub())
        stub->TrackMemoryAllocatedChange(delta);
    }

    uint64_t GetSize() const override {
      if (auto* stub = helper_->shared_image_stub())
        return stub->GetSize();
      return 0;
    }

    int ClientId() const override { return client_id_; }

    uint64_t ClientTracingId() const override { return client_tracing_id_; }

    uint64_t ContextGroupTracingId() const override {
      return context_group_tracing_id_;
    }

   private:
    const raw_ptr<CommandBufferHelperImpl> helper_;
    int client_id_ = 0;
    uint64_t client_tracing_id_ = 0;
    uint64_t context_group_tracing_id_ = 0;
  };

  ~CommandBufferHelperImpl() override {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (stub_) {
      DestroyStub();
    }
  }

  void OnWillDestroyStub(bool have_context) override {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (stub_) {
      DestroyStub();
    }
  }

  void DestroyStub() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // If the last reference to |this| is in a |done_cb|, destroying the wait
    // sequence can delete |this|. Clearing |stub_| first prevents DestroyStub()
    // being called twice.
    gpu::CommandBufferStub* stub = stub_;
    stub_ = nullptr;

    stub->RemoveDestructionObserver(this);
    stub->channel()->scheduler()->DestroySequence(wait_sequence_id_);
  }

  raw_ptr<gpu::CommandBufferStub> stub_;
  // Wait tasks are scheduled on our own sequence so that we can't inadvertently
  // block the command buffer.
  gpu::SequenceId wait_sequence_id_;

  MemoryTrackerImpl memory_tracker_;
  gpu::MemoryTypeTracker memory_type_tracker_;

  THREAD_CHECKER(thread_checker_);
};

CommandBufferHelper::CommandBufferHelper(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : base::RefCountedDeleteOnSequence<CommandBufferHelper>(
          std::move(task_runner)) {}

// static
scoped_refptr<CommandBufferHelper> CommandBufferHelper::Create(
    gpu::CommandBufferStub* stub) {
  return base::MakeRefCounted<CommandBufferHelperImpl>(stub);
}

}  // namespace media
