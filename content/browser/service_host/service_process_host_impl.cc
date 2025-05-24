// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/service_host/service_process_tracker.h"
#include "content/browser/service_host/utility_process_client.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {

namespace {

// Changes to this function should be reviewed by a security person.
bool ShouldEnableSandbox(sandbox::mojom::Sandbox sandbox) {
  if (sandbox == sandbox::mojom::Sandbox::kAudio) {
    return GetContentClient()->browser()->ShouldSandboxAudioService();
  }
  if (sandbox == sandbox::mojom::Sandbox::kNetwork) {
    return GetContentClient()->browser()->ShouldSandboxNetworkService();
  }
  return true;
}

// TODO(crbug.com/40633267): Once UtilityProcessHost is used only by service
// processes, its logic can be inlined here.
void LaunchServiceProcess(mojo::GenericPendingReceiver receiver,
                          ServiceProcessHost::Options service_options,
                          sandbox::mojom::Sandbox sandbox) {
  if (!ShouldEnableSandbox(sandbox)) {
    sandbox = sandbox::mojom::Sandbox::kNoSandbox;
  }
  UtilityProcessHost::Options utility_options;

  const auto service_interface_name = receiver.interface_name().value();

  utility_options
      .WithName(!service_options.display_name.empty()
                    ? service_options.display_name
                    : base::UTF8ToUTF16(service_interface_name))
      .WithMetricsName(service_interface_name)
      .WithSandboxType(sandbox)
      .WithExtraCommandLineSwitches(std::move(service_options.extra_switches));

  if (service_options.child_flags) {
    utility_options.WithChildFlags(*service_options.child_flags);
  }
#if BUILDFLAG(IS_WIN)
  if (!service_options.preload_libraries.empty()) {
    utility_options.WithPreloadLibraries(service_options.preload_libraries);
  }
#endif  // BUILDFLAG(IS_WIN)
  if (service_options.allow_gpu_client.has_value() &&
      service_options.allow_gpu_client.value()) {
    utility_options.WithGpuClientAllowed();
  }

  utility_options.WithBoundServiceInterfaceOnChildProcess(std::move(receiver));

  UtilityProcessHost::Start(std::move(utility_options),
                            std::make_unique<UtilityProcessClient>(
                                service_interface_name, service_options.site,
                                std::move(service_options.process_callback)));
}

}  // namespace

// static
std::vector<ServiceProcessInfo> ServiceProcessHost::GetRunningProcessInfo() {
  return GetServiceProcessTracker().GetProcesses();
}

// static
void ServiceProcessHost::AddObserver(Observer* observer) {
  GetServiceProcessTracker().AddObserver(observer);
}

// static
void ServiceProcessHost::RemoveObserver(Observer* observer) {
  GetServiceProcessTracker().RemoveObserver(observer);
}

// static
void ServiceProcessHost::Launch(mojo::GenericPendingReceiver receiver,
                                Options options,
                                sandbox::mojom::Sandbox sandbox) {
  DCHECK(receiver.interface_name().has_value());
  if (GetUIThreadTaskRunner({})->BelongsToCurrentThread()) {
    LaunchServiceProcess(std::move(receiver), std::move(options), sandbox);
  } else {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&LaunchServiceProcess, std::move(receiver),
                                  std::move(options), sandbox));
  }
}

}  // namespace content
