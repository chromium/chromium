// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/scoped_ipc_support.h"

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/ipcz_driver/transport.h"

namespace mojo::core {

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    ShutdownPolicy shutdown_policy)
    : shutdown_policy_(shutdown_policy) {
  ipcz_driver::Transport::SetIOTaskRunner(io_thread_task_runner);
}

ScopedIPCSupport::~ScopedIPCSupport() {
  if (IsMojoIpczEnabled()) {
    // No extra shutdown required for mojo-ipcz.
    // Suppress -Wunused-private-field warning, to not leak the buildflags
    // include into the header.
    (void)shutdown_policy_;
    return;
  }

  NOTREACHED();
}

}  // namespace mojo::core
