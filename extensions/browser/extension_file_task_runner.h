// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FILE_TASK_RUNNER_H_
#define EXTENSIONS_BROWSER_EXTENSION_FILE_TASK_RUNNER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"

namespace extensions {

// Returns the singleton instance of the task runner to be used for most
// extension-related tasks that read, modify, or delete files. All these tasks
// must be posted to this task runner, even if it is only reading the file,
// since other tasks may be modifying it.
scoped_refptr<base::SequencedTaskRunner> GetExtensionFileTaskRunner();

// Returns a non-singleton task runner, for tasks that touch files, but won't
// race with each other. Currently, this is used to unpack multiple extensions
// in parallel. They each touch a different set of files, which avoids potential
// race conditions.
scoped_refptr<base::SequencedTaskRunner> GetOneShotFileTaskRunner(
    base::TaskPriority priority);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FILE_TASK_RUNNER_H_
