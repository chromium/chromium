// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_
#define SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_

#include "base/functional/callback_forward.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// The current process will fork(). The parent will become a process reaper
// like init(1). The child will continue normally (after this function
// returns). If not empty, |post_fork_parent_callback| will run in the parent
// almost immediately after fork(). Since this function calls fork(), it's very
// important that the caller has only one thread running.
SANDBOX_EXPORT bool CreateInitProcessReaper(
    base::OnceClosure post_fork_parent_callback);

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_INIT_PROCESS_REAPER_H_
