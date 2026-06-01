// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_with_task_environment.h"

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "net/base/features.h"
#include "net/base/scheduler/net_task_priority.h"
#include "net/base/scheduler/net_task_scheduler.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/test/test_net_log_manager.h"

namespace net {

NetTaskEnvironment::NetTaskEnvironment(
    base::test::TaskEnvironment::MainThreadType main_thread_type,
    base::test::TaskEnvironment::TimeSource time_source)
    : base::test::TaskEnvironment(
          CreateNetTaskPrioritySettings(),
          base::test::TaskEnvironment::SubclassCreatesDefaultTaskRunner{},
          main_thread_type,
          time_source) {
  Init();
}

NetTaskEnvironment::~NetTaskEnvironment() = default;

void NetTaskEnvironment::Init() {
  if (base::FeatureList::IsEnabled(features::kNetTaskScheduler)) {
    scheduler_ =
        NetTaskScheduler::CreateForNetTaskEnvironment(sequence_manager());
    DeferredInitFromSubclass(scheduler_->GetDefaultTaskQueue());
  } else {
    // If the scheduler is disabled, we must still fulfill our promise to the
    // parent class (since we passed SubclassCreatesDefaultTaskRunner{} in the
    // constructor) by creating a default TaskQueue and registering it.
    // This replicates the default initialization logic inside the parent
    // constructor of base::test::TaskEnvironment when subclassing is not
    // deferred.
    default_task_queue_ = sequence_manager()->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            base::sequence_manager::QueueName::DEFAULT_TQ));
    DeferredInitFromSubclass(default_task_queue_.get());
  }
}

WithTaskEnvironment::FeatureDisabler::FeatureDisabler(
    const std::vector<base::test::FeatureRef>& disabled_features) {
  if (!disabled_features.empty()) {
    feature_list.InitWithFeatures({}, disabled_features);
  }
}

WithTaskEnvironment::WithTaskEnvironment(
    base::test::TaskEnvironment::TimeSource time_source,
    std::vector<base::test::FeatureRef> disabled_features)
    : feature_disabler_(disabled_features),
      task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                        time_source) {}

WithTaskEnvironment::~WithTaskEnvironment() = default;

}  // namespace net
