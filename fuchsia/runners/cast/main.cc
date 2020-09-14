// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "fuchsia/base/config_reader.h"
#include "fuchsia/base/feedback_registration.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/base/inspect.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

constexpr char kCrashProductName[] = "FuchsiaCastRunner";
// TODO(https://fxbug.dev/51490): Use a programmatic mechanism to obtain this.
constexpr char kComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx";

bool IsHeadless() {
  constexpr char kHeadlessConfigKey[] = "headless";

  const base::Optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (config)
    return config->FindBoolPath(kHeadlessConfigKey).value_or(false);

  return false;
}

bool AllowMainContextSharing() {
  constexpr char kAllowMainContextSharing[] = "enable-main-context-sharing";

  const base::Optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (config)
    return config->FindBoolPath(kAllowMainContextSharing).value_or(false);

  return false;
}

void LoadResources() {
  base::FilePath pak_file;
  bool result = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(result);
  pak_file = pak_file.Append(
      FILE_PATH_LITERAL("components/cast_api_bindings/"
                        "named_message_port_connector_resources.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  cr_fuchsia::RegisterCrashReportingFields(kComponentUrl, kCrashProductName);

  base::CommandLine::Init(argc, argv);
  CHECK(cr_fuchsia::InitLoggingFromCommandLine(
      *base::CommandLine::ForCurrentProcess()))
      << "Failed to initialize logging.";

  mojo::core::Init();

  LoadResources();

  cr_fuchsia::RegisterFuchsiaDirScheme();

  sys::OutgoingDirectory* const outgoing_directory =
      base::ComponentContextForProcess()->outgoing().get();

  // Publish the fuchsia.web.Runner implementation for Cast applications.
  CastRunner runner(IsHeadless());
  base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner> binding(
      outgoing_directory, &runner);

  // Optionally publish the fuchsia.web.FrameHost service, to allow the Cast
  // application web.Context to be shared by other components.
  base::Optional<base::fuchsia::ScopedServiceBinding<fuchsia::web::FrameHost>>
      frame_host_binding;
  if (AllowMainContextSharing()) {
    frame_host_binding.emplace(outgoing_directory,
                               runner.main_context_frame_host());
  }

  outgoing_directory->ServeFromStartupInfo();

  // Publish version information for this component to Inspect.
  cr_fuchsia::PublishVersionInfoToInspect(base::ComponentInspectorForProcess());

  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}
