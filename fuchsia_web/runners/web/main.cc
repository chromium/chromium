// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/fuchsia_component_support/inspect.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/runners/buildflags.h"
#include "fuchsia_web/runners/common/web_content_runner.h"
#include "fuchsia_web/webinstance_host/web_instance_host_v1.h"

namespace {

WebContentRunner::WebInstanceConfig GetWebInstanceConfig() {
  WebContentRunner::WebInstanceConfig config;

  config.params.set_features(
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);

  config.params.set_service_directory(
      base::OpenDirectoryHandle(base::FilePath(base::kServiceDirectoryPath)));
  CHECK(config.params.service_directory());

  config.params.set_data_directory(base::OpenDirectoryHandle(
      base::FilePath(base::kPersistedDataDirectoryPath)));
  CHECK(config.params.data_directory());

  // DRM services require cdm_data_directory to be populated, so create a
  // directory under /data and use that as the cdm_data_directory.
  base::FilePath cdm_data_path =
      base::FilePath(base::kPersistedDataDirectoryPath).Append("cdm_data");
  base::File::Error error;
  CHECK(base::CreateDirectoryAndGetError(cdm_data_path, &error)) << error;
  config.params.set_cdm_data_directory(
      base::OpenDirectoryHandle(cdm_data_path));
  CHECK(config.params.cdm_data_directory());

#if BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0
  config.params.set_remote_debugging_port(
      BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT));
#endif
  return config;
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  base::CommandLine::Init(argc, argv);
  CHECK(InitLoggingFromCommandLine(*base::CommandLine::ForCurrentProcess()))
      << "Failed to initialize logging.";

  LogComponentStartWithVersion("web_runner");

  WebInstanceHostV1 web_instance_host;
  WebContentRunner runner(web_instance_host,
                          base::BindRepeating(&GetWebInstanceConfig));
  base::ScopedServiceBinding<fuchsia::sys::Runner> binding(
      base::ComponentContextForProcess()->outgoing().get(), &runner);

  // Publish version information for this component to Inspect.
  sys::ComponentInspector inspect(base::ComponentContextForProcess());
  fuchsia_component_support::PublishVersionInfoToInspect(&inspect);

  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // Run until there are no Components, or the last service client channel is
  // closed.
  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}
