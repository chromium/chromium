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
                          ServiceProcessHost::Options options,
                          sandbox::mojom::Sandbox sandbox) {
  UtilityProcessHost* host =
      new UtilityProcessHost(std::make_unique<UtilityProcessClient>(
          *receiver.interface_name(), options.site,
          std::move(options.process_callback)));
  host->SetName(!options.display_name.empty()
                    ? options.display_name
                    : base::UTF8ToUTF16(*receiver.interface_name()));
  host->SetMetricsName(*receiver.interface_name());
  if (!ShouldEnableSandbox(sandbox)) {
    sandbox = sandbox::mojom::Sandbox::kNoSandbox;
  }
  host->SetSandboxType(sandbox);
  host->SetExtraCommandLineSwitches(std::move(options.extra_switches));
  if (options.child_flags) {
    host->set_child_flags(*options.child_flags);
  }
#if BUILDFLAG(IS_WIN)
  if (!options.preload_libraries.empty()) {
    host->SetPreloadLibraries(options.preload_libraries);
  }
#endif  // BUILDFLAG(IS_WIN)
  if (options.allow_gpu_client.has_value() &&
      options.allow_gpu_client.value()) {
    host->SetAllowGpuClient();
  }
  host->Start();
  host->GetChildProcess()->BindServiceInterface(std::move(receiver));
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
