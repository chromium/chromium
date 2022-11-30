// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/service_executable_environment.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/switches.h"
#include "services/service_manager/public/cpp/service_executable/switches.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/sandbox_type.h"
#endif

namespace service_manager {

ServiceExecutableEnvironment::ServiceExecutableEnvironment()
    : ipc_thread_("IPC Thread") {
  DCHECK(!base::CurrentThread::Get());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(sandbox::policy::switches::kServiceSandboxType)) {
    // Warm parts of base in the copy of base in the mojo runner.
    base::RandUint64();
    base::SysInfo::AmountOfPhysicalMemory();
    base::SysInfo::NumberOfProcessors();

    // Repeat steps normally performed by the zygote.
    sandbox::policy::SandboxLinux::Options sandbox_options;
    sandbox_options.engage_namespace_sandbox = true;

    sandbox::policy::Sandbox::Initialize(
        sandbox::policy::UtilitySandboxTypeFromString(
            command_line.GetSwitchValueASCII(
                sandbox::policy::switches::kServiceSandboxType)),
        sandbox::policy::SandboxLinux::PreSandboxHook(), sandbox_options);
  }
#endif

  mojo::core::Init();

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "StandaloneService");
  ipc_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  ipc_support_.emplace(ipc_thread_.task_runner(),
                       mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
}

ServiceExecutableEnvironment::~ServiceExecutableEnvironment() = default;

mojo::PendingReceiver<mojom::Service>
ServiceExecutableEnvironment::TakeServiceRequestFromCommandLine() {
  auto invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  return mojo::PendingReceiver<mojom::Service>(invitation.ExtractMessagePipe(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceRequestAttachmentName)));
}

}  // namespace service_manager
