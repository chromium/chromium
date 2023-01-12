// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>

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
#include "base/strings/string_piece.h"
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
#include "fuchsia_web/runners/cast/cast_runner_v1.h"
#include "fuchsia_web/webinstance_host/web_instance_host_v1.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Config-data key for launching Cast content without using Scenic.
constexpr char kHeadlessConfigKey[] = "headless";

// Config-data key for disable dynamic code generation by the web runtime.
constexpr char kDisableCodeGenConfigKey[] = "disable-codegen";

// Returns the value of |config_key| or false if it is not set.
bool GetConfigBool(base::StringPiece config_key) {
  const absl::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (config)
    return config->FindBool(config_key).value_or(false);
  return false;
}

// Name of the service capability implemented by the CFv2-based Runner.
constexpr char kCfv2RunnerService[] = "fuchsia.sys.Runner-cast";

// Publish a fuchsia.sys.Runner protocol that simply delegates to a specially-
// named protocol available in the incoming service directory.
int Cfv1ToCfv2RunnerProxyMain() {
  sys::OutgoingDirectory* const outgoing_directory =
      base::ComponentContextForProcess()->outgoing().get();

  const base::ScopedServicePublisher proxy_sys_runner(
      outgoing_directory,
      fidl::InterfaceRequestHandler<fuchsia::sys::Runner>(
          [](fidl::InterfaceRequest<fuchsia::sys::Runner> request) {
            zx_status_t status =
                base::ComponentContextForProcess()->svc()->Connect(
                    std::move(request), kCfv2RunnerService);
            ZX_CHECK(status == ZX_OK, status) << "Connect(Runner-cast)";
          }));

  // If the CFv2-based Runner implementation fails then terminate the proxy
  // so that the framework will observe this Runner-component failing.
  auto cfv2_runner =
      base::ComponentContextForProcess()->svc()->Connect<fuchsia::sys::Runner>(
          kCfv2RunnerService);
  CHECK(cfv2_runner) << "Connect(Runner-cast)";
  cfv2_runner.set_error_handler(
      base::LogFidlErrorAndExitProcess(FROM_HERE, kCfv2RunnerService));

  // Start serving the outgoing service directory to clients.
  outgoing_directory->ServeFromStartupInfo();

  // ELF runner will kill the component when the framework requests it to.
  base::RunLoop().Run();

  NOTREACHED();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  const bool enable_cfv2 = command_line->HasSwitch(kEnableCfv2);

  static constexpr base::StringPiece kComponentUrl(
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm");
  static constexpr base::StringPiece kComponentUrlCfv1(
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx");
  fuchsia_component_support::RegisterProductDataForCrashReporting(
      enable_cfv2 ? kComponentUrl : kComponentUrlCfv1, "FuchsiaCastRunner");

  CHECK(InitLoggingFromCommandLine(*command_line))
      << "Failed to initialize logging.";

  LogComponentStartWithVersion("cast_runner");

  // CastRunner is built even when `enable_cast_receiver=false` so that it can
  // always be tested. However, the statically linked WebEngineHost dependency
  // and WebEngine binary from the same build will be missing functionality and
  // should not be used with CastRunner outside tests.
#if !BUILDFLAG(ENABLE_CAST_RECEIVER)
  LOG(WARNING) << "This binary is from a build without Cast Receiver support "
                  "and does not support all necessary functionality.";
#endif

  if (!enable_cfv2) {
    return Cfv1ToCfv2RunnerProxyMain();
  }

  RegisterFuchsiaDirScheme();

  sys::OutgoingDirectory* const outgoing_directory =
      base::ComponentContextForProcess()->outgoing().get();

  // Publish the fuchsia.component.resolution.Resolver for the cast: scheme.
  CastResolver resolver;
  const base::ScopedServiceBinding<fuchsia::component::resolution::Resolver>
      resolver_binding(outgoing_directory, &resolver);

  // Publish the fuchsia.component.runner.ComponentRunner for Cast apps.
  WebInstanceHostV1 web_instance_host;
  CastRunner runner(
      web_instance_host,
      {.headless = command_line->HasSwitch(kForceHeadlessForTestsSwitch) ||
                   GetConfigBool(kHeadlessConfigKey),
       .disable_codegen = GetConfigBool(kDisableCodeGenConfigKey)});
  const base::ScopedServiceBinding<fuchsia::component::runner::ComponentRunner>
      runner_binding(outgoing_directory, &runner);

  // Publish the legacy fuchsia.sys.Runner implementation for Cast applications.
  CastRunnerV1 runner_v1;
  const base::ScopedServiceBinding<fuchsia::sys::Runner> runner_v1_binding(
      outgoing_directory, &runner_v1);

  // Publish the associated DataReset service for the instance.
  const base::ScopedServiceBinding<chromium::cast::DataReset>
      data_reset_binding(outgoing_directory, &runner);

  // Allow ephemeral web profiles to be created in the main web instance.
  const base::ScopedServicePublisher<fuchsia::web::FrameHost>
      frame_host_binding(outgoing_directory,
                         runner.GetFrameHostRequestHandler());

  // Allow web containers to be debugged, by end-to-end tests.
  base::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      outgoing_directory, web_instance_host.debug_api());

  if (command_line->HasSwitch(kDisableVulkanForTestsSwitch)) {
    runner.set_disable_vulkan_for_test();  // IN-TEST
  }

  // Publish version information for this component to Inspect.
  sys::ComponentInspector inspect(base::ComponentContextForProcess());
  fuchsia_component_support::PublishVersionInfoToInspect(&inspect);

  outgoing_directory->ServeFromStartupInfo();

  base::RunLoop run_loop;
  base::ProcessLifecycle process_lifecycle(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
