// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_main.h"

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
#include "fuchsia/base/feedback_registration.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/base/inspect.h"
#include "fuchsia/engine/context_provider_impl.h"

namespace {

// This must match the value in web_instance_host.cc
constexpr char kCrashProductName[] = "FuchsiaWebEngine";
// TODO(https://fxbug.dev/51490): Use a programmatic mechanism to obtain this.
constexpr char kComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cmx";

}  // namespace

int ContextProviderMain() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  cr_fuchsia::RegisterProductDataForCrashReporting(kComponentUrl,
                                                   kCrashProductName);

  if (!cr_fuchsia::InitLoggingFromCommandLine(
          *base::CommandLine::ForCurrentProcess())) {
    return 1;
  }

  cr_fuchsia::LogComponentStartWithVersion("WebEngine context_provider");

  ContextProviderImpl context_provider;

  // Publish the ContextProvider and Debug services.
  sys::OutgoingDirectory* const directory =
      base::ComponentContextForProcess()->outgoing().get();
  base::ScopedServiceBinding<fuchsia::web::ContextProvider> binding(
      directory, &context_provider);
  base::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      directory->debug_dir(), &context_provider);

  // Publish version information for this component to Inspect.
  sys::ComponentInspector inspect(base::ComponentContextForProcess());
  cr_fuchsia::PublishVersionInfoToInspect(&inspect);

  // Serve outgoing directory only after publishing all services.
  directory->ServeFromStartupInfo();

  // Graceful shutdown of the service is not required, so simply run the main
  // loop until the framework kills the process.
  base::RunLoop().Run();

  return 0;
}
