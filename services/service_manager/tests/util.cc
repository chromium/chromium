// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/util.h"

#include <optional>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_executable/switches.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/mach_port_rendezvous.h"
#endif

namespace service_manager {
namespace test {

namespace {

void GrabConnectResult(base::RunLoop* loop,
                       mojom::ConnectResult* out_result,
                       mojom::ConnectResult result) {
  loop->Quit();
  *out_result = result;
}

}  // namespace

mojom::ConnectResult LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity& target,
    service_manager::Connector* connector,
    base::Process* process) {
  // The test executable is a data_deps and thus generated test data.
  base::FilePath target_path;
  CHECK(base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &target_path));
  target_path = target_path.AppendASCII(target_exe_name);

  base::CommandLine child_command_line(target_path);
  // Forward the wait-for-debugger flag but nothing else - we don't want to
  // stamp on the platform-channel flag.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWaitForDebugger)) {
    child_command_line.AppendSwitch(::switches::kWaitForDebugger);
  }

  // Create the channel to be shared with the target process. Pass one end
  // on the command line.
  mojo::PlatformChannel channel;
  mojo::PlatformChannel::HandlePassingInfo handle_passing_info;
  channel.PrepareToPassRemoteEndpoint(&handle_passing_info,
                                      &child_command_line);

  mojo::OutgoingInvitation invitation;
  auto pipe_name = base::NumberToString(base::RandUint64());
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(pipe_name);
  child_command_line.AppendSwitchASCII(switches::kServiceRequestAttachmentName,
                                       pipe_name);

  mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;

  mojom::ConnectResult result;
  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  connector->RegisterServiceInstance(
      target,
      mojo::PendingRemote<service_manager::mojom::Service>(std::move(pipe), 0),
      metadata.BindNewPipeAndPassReceiver(),
      base::BindOnce(&GrabConnectResult, &loop, &result));
  loop.Run();

  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.handles_to_inherit = handle_passing_info;
#elif BUILDFLAG(IS_FUCHSIA)
  options.handles_to_transfer = handle_passing_info;
#elif BUILDFLAG(IS_MAC)
  options.mach_ports_for_rendezvous = handle_passing_info;
#elif BUILDFLAG(IS_POSIX)
  options.fds_to_remap = handle_passing_info;
#endif
  *process = base::LaunchProcess(child_command_line, options);
  DCHECK(process->IsValid());
  channel.RemoteProcessLaunchAttempted();
  metadata->SetPID(process->Pid());
  mojo::OutgoingInvitation::Send(std::move(invitation), process->Handle(),
                                 channel.TakeLocalEndpoint());
  return result;
}

}  // namespace test
}  // namespace service_manager
