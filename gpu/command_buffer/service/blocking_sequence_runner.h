// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_BLOCKING_SEQUENCE_RUNNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_BLOCKING_SEQUENCE_RUNNER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gpu_command_buffer_service_export.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/task_graph.h"

namespace gpu {

class Scheduler;

// BlockingSequenceRunner owns a TaskGraph::Sequence and supports running
// its tasks blockingly.
class GPU_COMMAND_BUFFER_SERVICE_EXPORT BlockingSequenceRunner {
 public:
  explicit BlockingSequenceRunner(Scheduler* scheduler) LOCKS_EXCLUDED(lock());

  BlockingSequenceRunner(const BlockingSequenceRunner&) = delete;
  BlockingSequenceRunner& operator=(const BlockingSequenceRunner&) = delete;

  ~BlockingSequenceRunner() LOCKS_EXCLUDED(lock());

  SequenceId GetSequenceId() const;

  bool HasTasks() const LOCKS_EXCLUDED(lock());

  uint32_t AddTask(TaskCallback task_calback,
                   std::vector<SyncToken> wait_fences,
                   const SyncToken& release,
                   ReportingCallback report_callback = {})
      LOCKS_EXCLUDED(lock());

  uint32_t AddTask(base::OnceClosure task_closure,
                   std::vector<SyncToken> wait_fences,
                   const SyncToken& release,
                   ReportingCallback report_callback = {})
      LOCKS_EXCLUDED(lock());

  [[nodiscard]] ScopedSyncPointClientState CreateSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock());

  // Runs tasks in the sequence blockingly until the sequence is empty.
  void RunAllTasks() LOCKS_EXCLUDED(lock());

 private:
  class Sequence : public TaskGraph::Sequence {
   public:
    explicit Sequence(Scheduler* scheduler);

    void RunAllTasks() EXCLUSIVE_LOCKS_REQUIRED(lock());

   private:
    const raw_ptr<Scheduler> scheduler_ = nullptr;
  };

  base::Lock& lock() const LOCK_RETURNED(task_graph_->lock()) {
    return task_graph_->lock();
  }

  const raw_ptr<TaskGraph> task_graph_ = nullptr;
  raw_ptr<Sequence> sequence_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_BLOCKING_SEQUENCE_RUNNER_H_
