// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"

#include "base/task/sequenced_task_runner.h"

namespace mojo {
namespace internal {

scoped_refptr<base::SequencedTaskRunner>
GetTaskRunnerToUseFromUserProvidedTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner) {
  if (runner)
    return runner;
  return base::SequencedTaskRunner::GetCurrentDefault();
}

}  // namespace internal
}  // namespace mojo
