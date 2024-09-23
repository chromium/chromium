// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/scoped_ipc_support.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/ipcz_driver/transport.h"

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#include "mojo/core/core.h"
#endif

namespace mojo::core {

namespace {

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
void ShutdownIPCSupport(base::OnceClosure callback) {
  Core::Get()->RequestShutdown(std::move(callback));
}
#endif

}  // namespace

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    ShutdownPolicy shutdown_policy)
    : shutdown_policy_(shutdown_policy) {
  ipcz_driver::Transport::SetIOTaskRunner(io_thread_task_runner);
#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  if (!IsMojoIpczEnabled()) {
    Core::Get()->SetIOTaskRunner(std::move(io_thread_task_runner));
  }
#endif
}

ScopedIPCSupport::~ScopedIPCSupport() {
  if (IsMojoIpczEnabled()) {
    // No extra shutdown required for mojo-ipcz.
    return;
  }

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  if (shutdown_policy_ == ShutdownPolicy::FAST) {
    ShutdownIPCSupport(base::DoNothing());
    return;
  }

  base::WaitableEvent shutdown_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ShutdownIPCSupport(base::BindOnce(&base::WaitableEvent::Signal,
                                    base::Unretained(&shutdown_event)));

  base::ScopedAllowBaseSyncPrimitives allow_io;
  shutdown_event.Wait();
#else
  NOTREACHED_NORETURN();
#endif
}

}  // namespace mojo::core
