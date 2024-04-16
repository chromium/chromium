// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/async/default.h>
#include <lib/inspect/component/cpp/component.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/trace-provider/provider.h>

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/process_lifecycle.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/config_reader.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "components/fuchsia_component_support/inspect.h"
#include "fuchsia_web/common/fuchsia_dir_scheme.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/runners/cast/cast_resolver.h"
#include "fuchsia_web/runners/cast/cast_runner.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"

namespace {

// Process ID used for logging and tracing. Should match the base name of the
// .cm file used by the cast_runner component.
constexpr char kProcessName[] = "cast_runner";

// Config-data key for launching Cast content without using Scenic.
constexpr char kHeadlessConfigKey[] = "headless";

// Config-data key for disable dynamic code generation by the web runtime.
constexpr char kDisableCodeGenConfigKey[] = "disable-codegen";

// Returns the value of |config_key| or false if it is not set.
bool GetConfigBool(std::string_view config_key) {
  const std::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (config)
    return config->FindBool(config_key).value_or(false);
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);

  static constexpr std::string_view kComponentUrl(
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm");
  fuchsia_component_support::RegisterProductDataForCrashReporting(
      kComponentUrl, "FuchsiaCastRunner");

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  CHECK(InitLoggingFromCommandLine(*command_line))
      << "Failed to initialize logging.";

  // Initialize tracing and logging.
  trace::TraceProviderWithFdio trace_provider(async_get_default_dispatcher(),
                                              kProcessName);
  LogComponentStartWithVersion(kProcessName);

  // CastRunner is built even when `enable_cast_receiver=false` so that it can
  // always be tested. However, the statically linked WebEngineHost dependency
  // and WebEngine binary from the same build will be missing functionality and
  // should not be used with CastRunner outside tests.
#if !BUILDFLAG(ENABLE_CAST_RECEIVER)
  LOG(WARNING) << "This binary is from a build without Cast Receiver support "
                  "and does not support all necessary functionality.";
#endif

  RegisterFuchsiaDirScheme();

  sys::OutgoingDirectory* const outgoing_directory =
      base::ComponentContextForProcess()->outgoing().get();

  // Publish the fuchsia.component.resolution.Resolver for the cast: scheme.
  CastResolver resolver;
  const base::ScopedNaturalServiceBinding resolver_binding(outgoing_directory,
                                                           &resolver);

  // Services from this component will be provided to each web instance.
  WebInstanceHostWithServicesFromThisComponent web_instance_host(
      *outgoing_directory,
      /*is_web_instance_component_in_same_package=*/false);

  // Publish the fuchsia.component.runner.ComponentRunner for Cast apps.
  CastRunner runner(
      web_instance_host,
      {.headless = command_line->HasSwitch(kForceHeadlessForTestsSwitch) ||
                   GetConfigBool(kHeadlessConfigKey),
       .disable_codegen = GetConfigBool(kDisableCodeGenConfigKey)});
  const base::ScopedServiceBinding<fuchsia::component::runner::ComponentRunner>
      runner_binding(outgoing_directory, &runner);

  // Publish the associated DataReset service for the instance.
  const base::ScopedServiceBinding<chromium::cast::DataReset>
      data_reset_binding(outgoing_directory, &runner);

  // Allow ephemeral web profiles to be created in the main web instance.
  const base::ScopedServicePublisher<fuchsia::web::FrameHost>
      frame_host_binding(outgoing_directory,
                         runner.GetFrameHostRequestHandler());

  // Allow web containers to be debugged, by end-to-end tests.
  base::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      outgoing_directory, &web_instance_host.debug_api());

  if (command_line->HasSwitch(kDisableVulkanForTestsSwitch)) {
    runner.set_disable_vulkan_for_test();  // IN-TEST
  }

  // Publish version information for this component to Inspect.
  inspect::ComponentInspector inspect(async_get_default_dispatcher(), {});
  fuchsia_component_support::PublishVersionInfoToInspect(&inspect.root());

  outgoing_directory->ServeFromStartupInfo();

  base::RunLoop run_loop;
  base::ProcessLifecycle process_lifecycle(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
