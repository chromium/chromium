// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_TASK_PUMP_H_
#define JINGLE_GLUE_TASK_PUMP_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/libjingle_xmpp/task_runner/taskrunner.h"

namespace jingle_glue {

// rtc::TaskRunner implementation that works on chromium threads.
class TaskPump : public jingle_xmpp::TaskRunner {
 public:
  TaskPump();

  ~TaskPump() override;

  // rtc::TaskRunner implementation.
  void WakeTasks() override;

  // No tasks will be processed after this is called, even if
  // WakeTasks() is called.
  void Stop();

 private:
  void CheckAndRunTasks();

  bool posted_wake_;
  bool stopped_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TaskPump> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TaskPump);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_TASK_PUMP_H_
