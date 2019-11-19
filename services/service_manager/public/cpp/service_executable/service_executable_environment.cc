// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/service_executable_environment.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service_executable/switches.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/sandbox/switches.h"

#if defined(OS_LINUX)
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#endif

namespace service_manager {

ServiceExecutableEnvironment::ServiceExecutableEnvironment()
    : ipc_thread_("IPC Thread") {
  DCHECK(!base::MessageLoopCurrent::Get());

#if defined(OS_LINUX)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kServiceSandboxType)) {
    // Warm parts of base in the copy of base in the mojo runner.
    base::RandUint64();
    base::SysInfo::AmountOfPhysicalMemory();
    base::SysInfo::NumberOfProcessors();

    // Repeat steps normally performed by the zygote.
    SandboxLinux::Options sandbox_options;
    sandbox_options.engage_namespace_sandbox = true;

    Sandbox::Initialize(
        UtilitySandboxTypeFromString(
            command_line.GetSwitchValueASCII(switches::kServiceSandboxType)),
        SandboxLinux::PreSandboxHook(), sandbox_options);
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

mojom::ServiceRequest
ServiceExecutableEnvironment::TakeServiceRequestFromCommandLine() {
  auto invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  return mojom::ServiceRequest(invitation.ExtractMessagePipe(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceRequestAttachmentName)));
}

}  // namespace service_manager
