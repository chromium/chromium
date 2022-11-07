// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_BACKEND_TASK_RUNNER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_BACKEND_TASK_RUNNER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace extensions {

// Gets the singleton task runner for running storage backend on.
scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner();

// Verifies the the backend task runner above runs tasks in the current
// sequence.
bool IsOnBackendSequence();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_BACKEND_TASK_RUNNER_H_
