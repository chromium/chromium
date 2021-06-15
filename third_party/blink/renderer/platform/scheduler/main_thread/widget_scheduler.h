// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_H_

#include <memory>

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/public/platform/scheduler/web_widget_scheduler.h"

namespace blink {
namespace scheduler {

class MainThreadSchedulerImpl;
class MainThreadTaskQueue;

class WidgetScheduler : public WebWidgetScheduler {
 public:
  WidgetScheduler(MainThreadSchedulerImpl*);
  WidgetScheduler(const WidgetScheduler&) = delete;
  WidgetScheduler& operator=(const WidgetScheduler&) = delete;
  ~WidgetScheduler() override;
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override;

 private:
  scoped_refptr<MainThreadTaskQueue> input_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>
      input_task_queue_enabled_voter_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_WIDGET_SCHEDULER_H_
