// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_TASK_RUNNER_HELPER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_TASK_RUNNER_HELPER_H_

#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace mojo {
namespace internal {

// Returns the SequencedTaskRunner to use from the optional user-provided
// SequencedTaskRunner. If |runner| is provided non-null, it is returned.
// Otherwise, SequencedTaskRunner::GetCurrentDefault() is returned. If |runner|
// is non-null, it must run tasks on the current sequence.
scoped_refptr<base::SequencedTaskRunner>
GetTaskRunnerToUseFromUserProvidedTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_TASK_RUNNER_HELPER_H_
