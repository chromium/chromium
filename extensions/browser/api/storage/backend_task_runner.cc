// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/backend_task_runner.h"

#include "base/task/sequenced_task_runner.h"
#include "extensions/browser/extension_file_task_runner.h"

namespace extensions {

// TODO(stanisc): consider switching all calls of GetBackendTaskRunner() to
// GetExtensionFileTaskRunner().
scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() {
  return GetExtensionFileTaskRunner();
}

bool IsOnBackendSequence() {
  return GetBackendTaskRunner()->RunsTasksInCurrentSequence();
}

}  // namespace extensions
