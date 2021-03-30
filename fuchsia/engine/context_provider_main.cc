// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_main.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/version_info/version_info.h"
#include "fuchsia/base/feedback_registration.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/base/inspect.h"
#include "fuchsia/base/lifecycle_impl.h"
#include "fuchsia/engine/context_provider_impl.h"

namespace {

constexpr char kFeedbackAnnotationsNamespace[] = "web-engine";
constexpr char kCrashProductName[] = "FuchsiaWebEngine";
// TODO(https://fxbug.dev/51490): Use a programmatic mechanism to obtain this.
constexpr char kComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cmx";

std::string GetVersionString() {
  std::string version_string = version_info::GetVersionNumber();
#if !defined(OFFICIAL_BUILD)
  version_string += " (built at " + version_info::GetLastChange() + ")";
#endif  // !defined(OFFICIAL_BUILD)
  return version_string;
}

}  // namespace

int ContextProviderMain() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  cr_fuchsia::RegisterProductDataForCrashReporting(kComponentUrl,
                                                   kCrashProductName);

  if (!cr_fuchsia::InitLoggingFromCommandLine(
          *base::CommandLine::ForCurrentProcess())) {
    return 1;
  }

  // Populate feedback annotations for this component.
  // TODO(crbug.com/1010222): Add annotations at Context startup, once Contexts
  // are moved out to run in their own components.
  cr_fuchsia::RegisterProductDataForFeedback(kFeedbackAnnotationsNamespace);

  LOG(INFO) << "Starting WebEngine " << GetVersionString();

  ContextProviderImpl context_provider;

  // Publish the ContextProvider and Debug services.
  sys::OutgoingDirectory* const directory =
      base::ComponentContextForProcess()->outgoing().get();
  base::ScopedServiceBinding<fuchsia::web::ContextProvider> binding(
      directory, &context_provider);
  base::ScopedServiceBinding<fuchsia::web::Debug> debug_binding(
      directory->debug_dir(), &context_provider);

  // Publish version information for this component to Inspect.
  cr_fuchsia::PublishVersionInfoToInspect(base::ComponentInspectorForProcess());

  // Publish the Lifecycle service, used by the framework to request that the
  // service terminate.
  base::RunLoop run_loop;
  cr_fuchsia::LifecycleImpl lifecycle(directory, run_loop.QuitClosure());

  // Serve outgoing directory only after publishing all services.
  directory->ServeFromStartupInfo();

  run_loop.Run();

  return 0;
}
