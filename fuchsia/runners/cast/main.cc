// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>

#include "base/command_line.h"
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
#include "fuchsia/base/config_reader.h"
#include "fuchsia/base/feedback_registration.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/base/inspect.h"
#include "fuchsia/engine/web_instance_host/web_instance_host.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/cast_runner_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Config-data key for launching Cast content without using Scenic.
constexpr char kHeadlessConfigKey[] = "headless";

// Config-data key to enable the fuchsia.web.FrameHost provider component.
constexpr char kFrameHostConfigKey[] = "enable-frame-host-component";

// Config-data key to run the CFv1 runner as a shim to the CFv2 runner.
constexpr char kRunCfv1ShimConfigKey[] = "run-cfv1-shim";

// Returns the value of |config_key| or false if it is not set.
bool GetConfigBool(base::StringPiece config_key) {
  const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (config)
    return config->FindBoolPath(config_key).value_or(false);
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  const bool enable_cfv2 = command_line->HasSwitch(kEnableCfv2);

  static constexpr base::StringPiece kComponentUrl(
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cm");
  static constexpr base::StringPiece kComponentUrlCfv1(
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx");
  cr_fuchsia::RegisterProductDataForCrashReporting(
      enable_cfv2 ? kComponentUrl : kComponentUrlCfv1, "FuchsiaCastRunner");

  CHECK(cr_fuchsia::InitLoggingFromCommandLine(*command_line))
      << "Failed to initialize logging.";

  cr_fuchsia::LogComponentStartWithVersion("cast_runner");

  cr_fuchsia::RegisterFuchsiaDirScheme();

  sys::OutgoingDirectory* const outgoing_directory =
      base::ComponentContextForProcess()->outgoing().get();

  if (!enable_cfv2 && GetConfigBool(kRunCfv1ShimConfigKey)) {
    // TODO(crbug.com/1065707): Delegate the Runner protocol to the CFv2 runner.
    NOTIMPLEMENTED();
    return 1;
  }

  // Publish the fuchsia.sys.Runner implementation for Cast applications.
  cr_fuchsia::WebInstanceHost web_instance_host;
  const bool enable_headless =
      command_line->HasSwitch(kForceHeadlessForTestsSwitch) ||
      GetConfigBool(kHeadlessConfigKey);
  CastRunner runner(&web_instance_host, enable_headless);
  base::ScopedServiceBinding<fuchsia::sys::Runner> binding(outgoing_directory,
                                                           &runner);

  if (command_line->HasSwitch(kDisableVulkanForTestsSwitch)) {
    runner.set_disable_vulkan_for_test();  // IN-TEST
  }

  // Optionally enable a pseudo-component providing the fuchsia.web.FrameHost
  // service, to allow the Cast application web.Context to be shared by other
  // components.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kFrameHostConfigKey) ||
      GetConfigBool(kFrameHostConfigKey)) {
    runner.set_enable_frame_host_component();
  }

  // Publish version information for this component to Inspect.
  sys::ComponentInspector inspect(base::ComponentContextForProcess());
  cr_fuchsia::PublishVersionInfoToInspect(&inspect);

  outgoing_directory->ServeFromStartupInfo();

  base::RunLoop run_loop;
  absl::optional<base::ProcessLifecycle> process_lifecycle;

  if (enable_cfv2)
    process_lifecycle.emplace(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
