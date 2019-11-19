// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_TASK_QUEUE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_TASK_QUEUE_CONTROLLER_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
}  // namespace sequence_manager
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class FrameSchedulerImpl;
class FrameTaskQueueControllerTest;
class MainThreadSchedulerImpl;

// FrameTaskQueueController creates and manages and FrameSchedulerImpl's task
// queues. It is in charge of maintaining mappings between QueueTraits and
// MainThreadTaskQueues for queues, for accessing task queues and
// their related voters, and for creating new task queues.
class PLATFORM_EXPORT FrameTaskQueueController {
  USING_FAST_MALLOC(FrameTaskQueueController);

 public:
  using TaskQueueAndEnabledVoterPair =
      std::pair<MainThreadTaskQueue*,
                base::sequence_manager::TaskQueue::QueueEnabledVoter*>;

  // Delegate receives callbacks when task queues are created to perform any
  // additional task queue setup or tasks.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnTaskQueueCreated(
        MainThreadTaskQueue*,
        base::sequence_manager::TaskQueue::QueueEnabledVoter*) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  FrameTaskQueueController(MainThreadSchedulerImpl*,
                           FrameSchedulerImpl*,
                           Delegate*);
  ~FrameTaskQueueController();

  // Return the task queue associated with the given queue traits,
  // and create if it doesn't exist.
  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(
      MainThreadTaskQueue::QueueTraits);

  scoped_refptr<MainThreadTaskQueue> NewResourceLoadingTaskQueue();

  scoped_refptr<MainThreadTaskQueue> NewWebSchedulingTaskQueue(
      MainThreadTaskQueue::QueueTraits,
      WebSchedulingPriority);

  // Get the list of all task queue and voter pairs.
  const Vector<TaskQueueAndEnabledVoterPair>& GetAllTaskQueuesAndVoters() const;

  // Gets the associated QueueEnabledVoter for the given task queue, or nullptr
  // if one doesn't exist.
  base::sequence_manager::TaskQueue::QueueEnabledVoter* GetQueueEnabledVoter(
      const scoped_refptr<MainThreadTaskQueue>&);

  // Remove a resource loading task queue that FrameTaskQueueController created,
  // along with its QueueEnabledVoter, if one exists. Returns true if the task
  // queue was found and erased and false otherwise.
  //
  // Removes are linear in the total number of task queues since
  // |all_task_queues_and_voters_| needs to be updated.
  bool RemoveResourceLoadingTaskQueue(
      const scoped_refptr<MainThreadTaskQueue>&);

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  friend class FrameTaskQueueControllerTest;

  void CreateTaskQueue(MainThreadTaskQueue::QueueTraits);

  void TaskQueueCreated(const scoped_refptr<MainThreadTaskQueue>&);

  // Map a set of QueueTraits to a QueueType.
  // TODO(crbug.com/877245): Consider creating a new queue type kFrameNonLoading
  // and use it instead of this for new queue types.
  static MainThreadTaskQueue::QueueType QueueTypeFromQueueTraits(
      MainThreadTaskQueue::QueueTraits);

  MainThreadSchedulerImpl* const main_thread_scheduler_impl_;
  FrameSchedulerImpl* const frame_scheduler_impl_;
  Delegate* const delegate_;

  using TaskQueueMap =
      WTF::HashMap<MainThreadTaskQueue::QueueTraitsKeyType,
                   scoped_refptr<MainThreadTaskQueue>>;

  // Map of all TaskQueues, indexed by QueueTraits.
  TaskQueueMap task_queues_;

  // Set of all resource loading task queues.
  WTF::HashSet<scoped_refptr<MainThreadTaskQueue>>
      resource_loading_task_queues_;

  using TaskQueueEnabledVoterMap = WTF::HashMap<
      scoped_refptr<MainThreadTaskQueue>,
      std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>>;

  // QueueEnabledVoters for the task queues we've created. Note: Some task
  // queues do not have an associated voter.
  TaskQueueEnabledVoterMap task_queue_enabled_voters_;

  // The list of all task queue and voter pairs for all QueueTypeInternal queue
  // types.
  Vector<TaskQueueAndEnabledVoterPair> all_task_queues_and_voters_;

  DISALLOW_COPY_AND_ASSIGN(FrameTaskQueueController);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_TASK_QUEUE_CONTROLLER_H_
