// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/entrypoints.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/thunks.h"

namespace {

class IPCSupport {
 public:
  IPCSupport() : ipc_thread_("Mojo IPC") {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    ipc_thread_.StartWithOptions(std::move(options));
    mojo::core::Core::Get()->SetIOTaskRunner(ipc_thread_.task_runner());
  }

  IPCSupport(const IPCSupport&) = delete;
  IPCSupport& operator=(const IPCSupport&) = delete;

  ~IPCSupport() {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    mojo::core::Core::Get()->RequestShutdown(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&wait)));
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
};

std::unique_ptr<IPCSupport>& GetIPCSupport() {
  static base::NoDestructor<std::unique_ptr<IPCSupport>> state;
  return *state;
}

// This helper is only called from within the context of a newly loaded Mojo
// Core shared library, where various bits of static state (e.g. //base globals)
// will not yet be initialized. Base library initialization steps are thus
// consolidated here so that base APIs work as expected from within the loaded
// Mojo Core implementation.
//
// NOTE: This is a no-op in component builds, as we expect both the client
// application and the Mojo Core library to have been linked against the same
// base component library, and we furthermore expect that the client application
// has already initialized base globals by this point.
class GlobalStateInitializer {
 public:
  GlobalStateInitializer() = default;
  ~GlobalStateInitializer() = delete;

  bool Initialize(int argc, const char* const* argv) {
    if (initialized_)
      return false;
    initialized_ = true;
#if !defined(COMPONENT_BUILD)
    base::CommandLine::Init(argc, argv);

    logging::LoggingSettings settings;
    settings.logging_dest =
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
    logging::InitLogging(settings);
    logging::SetLogItems(true,   // Process ID
                         true,   // Thread ID
                         true,   // Timestamp
                         true);  // Tick count

#if !defined(OFFICIAL_BUILD) && !BUILDFLAG(IS_WIN)
    // Correct stack dumping behavior requires symbol names in all loaded
    // libraries to be cached. We do this here in case the calling process will
    // imminently enter a sandbox.
    base::debug::EnableInProcessStackDumping();
#endif

#if BUILDFLAG(IS_POSIX)
    // Tickle base's PRNG. This lazily opens a static handle to /dev/urandom.
    // Mojo Core uses the API internally, so it's important to warm the handle
    // before potentially entering a sandbox.
    base::RandUint64();
#endif

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // If FieldTrialList::GetInstance() returns nullptr,
    // FeatureList::InitFromCommandLine(), used by FeatureList::
    // InitInstance internally, creates no field trials. This causes
    // DCHECK() failure if we have an command line like
    // --enable-features=TestFeature:TestParam/TestValue.
    // We don't need to care about FieldTrialList duplication here, because
    // this code is available for static build. If base library is not shared,
    // libmojo_core.so and the caller of LoadAndInitializeCoreLibrary doesn't
    // share FieldTrialList::GetInstance().
    field_trial_list_ = std::make_unique<base::FieldTrialList>();
    base::FeatureList::InitInstance(
        command_line->GetSwitchValueASCII(switches::kEnableFeatures),
        command_line->GetSwitchValueASCII(switches::kDisableFeatures));
#endif  // !defined(COMPONENT_BUILD)
    return true;
  }

 private:
  bool initialized_ = false;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

}  // namespace

extern "C" {

namespace {

MojoResult InitializeImpl(const struct MojoInitializeOptions* options) {
  std::unique_ptr<IPCSupport>& ipc_support = GetIPCSupport();
  if (ipc_support) {
    // Already fully initialized, so there's nothing to do.
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // NOTE: |MojoInitialize()| may be called more than once if the caller wishes
  // to separate basic initialization from IPC support initialization. We only
  // do basic initialization the first time this is called.
  const bool should_initialize_ipc_support =
      !options || ((options->flags & MOJO_INITIALIZE_FLAG_LOAD_ONLY) == 0);

  int argc = 0;
  const char* const* argv = nullptr;
  if (options && MOJO_IS_STRUCT_FIELD_PRESENT(options, argv)) {
    argc = options->argc;
    argv = options->argv;
  }

  static base::NoDestructor<GlobalStateInitializer> global_state_initializer;
  const bool was_global_state_already_initialized =
      !global_state_initializer->Initialize(argc, argv);

  if (!should_initialize_ipc_support) {
    if (was_global_state_already_initialized)
      return MOJO_RESULT_ALREADY_EXISTS;
    else
      return MOJO_RESULT_OK;
  }

  DCHECK(!mojo::core::Core::Get());
  mojo::core::Configuration config;
  config.is_broker_process =
      options && options->flags & MOJO_INITIALIZE_FLAG_AS_BROKER;
  config.force_direct_shared_memory_allocation =
      options && options->flags &
                     MOJO_INITIALIZE_FLAG_FORCE_DIRECT_SHARED_MEMORY_ALLOCATION;
  mojo::core::internal::g_configuration = config;
  mojo::core::InitializeCore();
  ipc_support = std::make_unique<IPCSupport>();

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

MojoSystemThunks2 g_thunks = {0};

}  // namespace

#if defined(WIN32)
#define EXPORT_FROM_MOJO_CORE __declspec(dllexport)
#else
#define EXPORT_FROM_MOJO_CORE __attribute__((visibility("default")))
#endif

EXPORT_FROM_MOJO_CORE void MojoGetSystemThunks(MojoSystemThunks2* thunks) {
  if (!g_thunks.size) {
    g_thunks = mojo::core::GetSystemThunks();
    g_thunks.Initialize = InitializeImpl;
    g_thunks.Shutdown = ShutdownImpl;
  }

  // Caller must provide a thunk structure at least large enough to hold Core
  // ABI version 0. SetQuota is the first function introduced in ABI version 1.
  CHECK_GE(thunks->size, offsetof(MojoSystemThunks2, SetQuota));

  // NOTE: This also overrites |thunks->size| with the actual size of our own
  // thunks if smaller than the caller's. This informs the caller that we
  // implement an older version of the ABI.
  if (thunks->size > g_thunks.size)
    thunks->size = g_thunks.size;
  memcpy(thunks, &g_thunks, thunks->size);
}

}  // extern "C"
