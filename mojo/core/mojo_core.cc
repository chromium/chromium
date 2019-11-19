// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/entrypoints.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/thunks.h"

namespace {

class IPCSupport {
 public:
  IPCSupport() : ipc_thread_("Mojo IPC") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    ipc_thread_.StartWithOptions(options);
    mojo::core::Core::Get()->SetIOTaskRunner(ipc_thread_.task_runner());
  }

  ~IPCSupport() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    mojo::core::Core::Get()->RequestShutdown(base::BindRepeating(
        &base::WaitableEvent::Signal, base::Unretained(&wait)));
    wait.Wait();
  }

 private:
#if !defined(COMPONENT_BUILD)
  // NOTE: For component builds, we assume the consumer is always a target in
  // the Chromium tree which already depends on base initialization stuff and
  // therefore already has an AtExitManager. For non-component builds, use of
  // this AtExitManager is strictly isolated to Mojo Core internals, so running
  // hooks on |MojoShutdown()| (where |this| is destroyed) makes sense.
  base::AtExitManager at_exit_manager_;
#endif  // !defined(COMPONENT_BUILD)

  base::Thread ipc_thread_;

  DISALLOW_COPY_AND_ASSIGN(IPCSupport);
};

std::unique_ptr<IPCSupport>& GetIPCSupport() {
  static base::NoDestructor<std::unique_ptr<IPCSupport>> state;
  return *state;
}

}  // namespace

extern "C" {

namespace {

MojoResult InitializeImpl(const struct MojoInitializeOptions* options) {
  mojo::core::Configuration config;
  config.is_broker_process =
      options && options->flags & MOJO_INITIALIZE_FLAG_AS_BROKER;
  mojo::core::internal::g_configuration = config;

  mojo::core::InitializeCore();
  GetIPCSupport() = std::make_unique<IPCSupport>();
  return MOJO_RESULT_OK;
}

MojoResult ShutdownImpl(const struct MojoShutdownOptions* options) {
  if (options && options->struct_size < sizeof(*options))
    return MOJO_RESULT_INVALID_ARGUMENT;

  std::unique_ptr<IPCSupport>& ipc_support = GetIPCSupport();
  if (!ipc_support)
    return MOJO_RESULT_FAILED_PRECONDITION;

  ipc_support.reset();
  return MOJO_RESULT_OK;
}

MojoSystemThunks g_thunks = {0};

}  // namespace

#if defined(WIN32)
#define EXPORT_FROM_MOJO_CORE __declspec(dllexport)
#else
#define EXPORT_FROM_MOJO_CORE __attribute__((visibility("default")))
#endif

EXPORT_FROM_MOJO_CORE void MojoGetSystemThunks(MojoSystemThunks* thunks) {
  if (!g_thunks.size) {
    g_thunks = mojo::core::GetSystemThunks();
    g_thunks.Initialize = InitializeImpl;
    g_thunks.Shutdown = ShutdownImpl;
  }

  // Caller must provide a thunk structure at least large enough to hold Core
  // ABI version 0. SetQuota is the first function introduced in ABI version 1.
  CHECK_GE(thunks->size, offsetof(MojoSystemThunks, SetQuota));

  // NOTE: This also overrites |thunks->size| with the actual size of our own
  // thunks if smaller than the caller's. This informs the caller that we
  // implement an older version of the ABI.
  if (thunks->size > g_thunks.size)
    thunks->size = g_thunks.size;
  memcpy(thunks, &g_thunks, thunks->size);
}

}  // extern "C"
