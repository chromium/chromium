// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/context_provider_main.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/inspect/cpp/component.h>

#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "components/fuchsia_component_support/inspect.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/webengine/context_provider_impl.h"
#include "fuchsia_web/webengine/switches.h"

namespace {

// LINT.IfChange(web_engine_crash_product_name)
// This must match the value in web_instance_host.cc
constexpr char kCrashProductName[] = "FuchsiaWebEngine";
// LINT.ThenChange(//fuchsia_web/webinstance_host/web_instance_host.cc:web_engine_crash_product_name)

// The URL cannot be obtained programmatically - see fxbug.dev/51490.
constexpr char kComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cm";
constexpr char kComponentUrlCfv1[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cmx";

}  // namespace

int ContextProviderMain() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  // Register with crash reporting, under the appropriate component URL.
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  const bool enable_cfv2 = command_line->HasSwitch(switches::kEnableCfv2);
  fuchsia_component_support::RegisterProductDataForCrashReporting(
      enable_cfv2 ? kComponentUrl : kComponentUrlCfv1, kCrashProductName);

  if (!InitLoggingFromCommandLine(*command_line)) {
    return 1;
  }

  LogComponentStartWithVersion("WebEngine context_provider");

  ContextProviderImpl context_provider;

  // Publish the ContextProvider and Debug services.
  sys::OutgoingDirectory* const directory =
      base::ComponentContextForProcess()->outgoing().get();
  base::ScopedServiceBinding<fuchsia::web::ContextProvider> binding(
      directory, &context_provider);
  base::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      directory->debug_dir(), context_provider.debug_api());

  absl::optional<base::ScopedServiceBinding<fuchsia::web::Debug>>
      debug_binding_cfv2;
  if (enable_cfv2) {
    debug_binding_cfv2.emplace(directory, context_provider.debug_api());
  }

  // Publish version information for this component to Inspect.
  sys::ComponentInspector inspect(base::ComponentContextForProcess());
  fuchsia_component_support::PublishVersionInfoToInspect(&inspect);

  // Serve outgoing directory only after publishing all services.
  directory->ServeFromStartupInfo();

  // Graceful shutdown of the service is not required, so simply run the main
  // loop until the framework kills the process.
  base::RunLoop().Run();

  return 0;
}
