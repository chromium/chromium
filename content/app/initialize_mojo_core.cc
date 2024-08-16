// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/initialize_mojo_core.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "sandbox/policy/sandbox_type.h"

namespace content {

void InitializeMojoCore() {
  if (base::FeatureList::GetInstance()) {
    mojo::core::InitFeatures();
  } else {
    DLOG(FATAL) << "Initializing Mojo without a FeatureList";
  }

  mojo::core::Configuration config;
  config.max_message_num_bytes = 128 * 1024 * 1024;

  // If this is the browser process and there's no Mojo invitation pipe on the
  // command line, we will serve as the global Mojo broker.
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  const bool is_browser = !command_line.HasSwitch(switches::kProcessType);
  if (is_browser) {
    // On Lacros, Chrome is not always the broker, because ash-chrome is.
    // Otherwise, look at the command line flag to decide whether it is
    // a broker.
    config.is_broker_process =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        false
#else
        !command_line.HasSwitch(switches::kDisableMojoBroker) &&
        !mojo::PlatformChannel::CommandLineHasPassedEndpoint(command_line)
#endif
        ;
    if (!config.is_broker_process)
      config.force_direct_shared_memory_allocation = true;
  } else {
#if BUILDFLAG(IS_WIN)
    // On Windows it's not necessary to broker shared memory allocation, as
    // even sandboxed processes can allocate their own without trouble.
    config.force_direct_shared_memory_allocation = true;
#endif
  }

  mojo::core::Init(config);

  // Note #1: the installed shared memory hooks require a live instance of
  // mojo::core::ScopedIPCSupport to function, which is instantiated below by
  // the process type's main function. However, some Content embedders
  // allocate within the ContentMainRunner::Initialize call above, so the
  // hooks cannot be installed before that or the shared memory allocation
  // will simply fail.
  //
  // Note #2: some platforms can directly allocated shared memory in a
  // sandboxed process. The defines below must be in sync with the
  // implementation of mojo::NodeController::CreateSharedBuffer().
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA) && \
    !BUILDFLAG(IS_ANDROID)
  if (sandbox::policy::IsUnsandboxedSandboxType(
          sandbox::policy::SandboxTypeFromCommandLine(
              *base::CommandLine::ForCurrentProcess()))) {
    // Unsandboxed processes don't need shared memory brokering... because
    // they're not sandboxed.
  } else if (config.force_direct_shared_memory_allocation) {
    // Don't bother with hooks if direct shared memory allocation has been
    // requested.
  } else {
    // Sanity check, since installing the shared memory hooks in a broker
    // process will lead to infinite recursion.
    DCHECK(!config.is_broker_process);
    // Otherwise, this is a sandboxed process that will need brokering to
    // allocate shared memory.
    mojo::SharedMemoryUtils::InstallBaseHooks();
  }
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
}

}  // namespace content
