// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/buildflag.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/cast/cast_runner.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  base::CommandLine::Init(argc, argv);
  if (!cr_fuchsia::InitLoggingFromCommandLine(
          *base::CommandLine::ForCurrentProcess())) {
    return 1;
  }

  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;

  if (!BUILDFLAG(ENABLE_SOFTWARE_VIDEO_DECODERS))
    features |= fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;

  fuchsia::web::CreateContextParams create_context_params =
      WebContentRunner::BuildCreateContextParams(
          fidl::InterfaceHandle<fuchsia::io::Directory>(), features);

  const char kCastPlayreadyKeySystem[] = "com.chromecast.playready";
  create_context_params.set_playready_key_system(kCastPlayreadyKeySystem);

  // TODO(b/141956135): Use CrKey version provided by the Agent.
  create_context_params.set_user_agent_product("CrKey");
  create_context_params.set_user_agent_version("0");

  const uint16_t kRemoteDebuggingPort = 9222;
  create_context_params.set_remote_debugging_port(kRemoteDebuggingPort);

  // TODO(crbug.com/1023514): Remove this switch when it is no longer
  // necessary.
  create_context_params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content"});

  CastRunner runner(
      base::fuchsia::ComponentContextForCurrentProcess()->outgoing().get(),
      std::move(create_context_params));

  base::fuchsia::ComponentContextForCurrentProcess()
      ->outgoing()
      ->ServeFromStartupInfo();

  // Run until there are no Components, or the last service client channel is
  // closed.
  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}
