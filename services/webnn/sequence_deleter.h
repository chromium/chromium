// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_SEQUENCE_DELETER_H_
#define SERVICES_WEBNN_SEQUENCE_DELETER_H_

#include "base/component_export.h"
#include "base/task/sequenced_task_runner.h"

namespace webnn {

// Similar to base::OnTaskRunnerDeleter but deletes the object immediately when
// executed on the task runner’s sequence instead of posting a task.
// This is useful for non-shared objects that must be deleted on a specific
// sequence.
struct COMPONENT_EXPORT(WEBNN_SERVICE) OnTaskRunnerDeleter {
  explicit OnTaskRunnerDeleter(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~OnTaskRunnerDeleter();

  OnTaskRunnerDeleter(OnTaskRunnerDeleter&&);
  OnTaskRunnerDeleter& operator=(OnTaskRunnerDeleter&&);

  // For compatibility with std:: deleters.
  template <typename T>
  void operator()(const T* ptr) {
    if (!ptr) {
      return;
    }
    if (task_runner_->RunsTasksInCurrentSequence()) {
      delete ptr;
    } else {
      task_runner_->DeleteSoon(FROM_HERE, ptr);
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_SEQUENCE_DELETER_H_
