// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace mojo {
namespace internal {

scoped_refptr<base::SequencedTaskRunner>
GetTaskRunnerToUseFromUserProvidedTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner) {
  if (runner)
    return runner;
  return base::SequencedTaskRunnerHandle::Get();
}

}  // namespace internal
}  // namespace mojo
