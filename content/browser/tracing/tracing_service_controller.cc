// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_service_controller.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/tracing_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/tracing/public/cpp/traced_process.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "services/tracing/tracing_service.h"

namespace content {

namespace {

void BindNewInProcessInstance(
    mojo::PendingReceiver<tracing::mojom::TracingService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<tracing::TracingService>(),
                              std::move(receiver));
}

}  // namespace

TracingServiceController::ClientRegistration::ClientRegistration(
    base::PassKey<TracingServiceController>,
    base::OnceClosure unregister)
    : unregister_(std::move(unregister)) {}

TracingServiceController::ClientRegistration::~ClientRegistration() {
  std::move(unregister_).Run();
}

TracingServiceController::TracingServiceController() = default;

TracingServiceController::~TracingServiceController() = default;

// static
TracingServiceController& TracingServiceController::Get() {
  static base::NoDestructor<TracingServiceController> controller;
  return *controller;
}

std::unique_ptr<TracingServiceController::ClientRegistration>
TracingServiceController::RegisterClient(base::ProcessId pid,
                                         EnableTracingCallback callback) {
  base::OnceClosure unregister =
      base::BindOnce(&TracingServiceController::RemoveClient,
                     base::Unretained(&TracingServiceController::Get()), pid);
  auto registration = std::make_unique<ClientRegistration>(
      base::PassKey<TracingServiceController>(), std::move(unregister));

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Force registration to happen on the UI thread.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TracingServiceController::RegisterClientOnUIThread,
                       base::Unretained(this), pid, std::move(callback)));
  } else {
    RegisterClientOnUIThread(pid, std::move(callback));
  }

  return registration;
}

tracing::mojom::TracingService& TracingServiceController::GetService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_) {
    auto receiver = service_.BindNewPipeAndPassReceiver();
    if (base::FeatureList::IsEnabled(features::kTracingServiceInProcess)) {
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::WithBaseSyncPrimitives(), base::TaskPriority::USER_BLOCKING})
          ->PostTask(FROM_HERE, base::BindOnce(&BindNewInProcessInstance,
                                               std::move(receiver)));
    } else {
      ServiceProcessHost::Launch(
          std::move(receiver),
          ServiceProcessHost::Options()
              .WithDisplayName("Tracing Service")
              .Pass());
    }
    service_.reset_on_disconnect();

    // Initialize the new service instance by pushing a pipe to each currently
    // registered client, including the browser process itself.
    std::vector<tracing::mojom::ClientInfoPtr> initial_clients;
    mojo::PendingRemote<tracing::mojom::TracedProcess> browser_remote;
    tracing::TracedProcess::ResetTracedProcessReceiver();
    tracing::TracedProcess::OnTracedProcessRequest(
        browser_remote.InitWithNewPipeAndPassReceiver());
    initial_clients.push_back(tracing::mojom::ClientInfo::New(
        base::GetCurrentProcId(), std::move(browser_remote)));
    for (const std::pair<const base::ProcessId, EnableTracingCallback>& entry :
         clients_) {
      mojo::PendingRemote<tracing::mojom::TracedProcess> remote_process;
      entry.second.Run(remote_process.InitWithNewPipeAndPassReceiver());
      initial_clients.push_back(tracing::mojom::ClientInfo::New(
          /*pid=*/entry.first, std::move(remote_process)));
    }
    service_->Initialize(std::move(initial_clients));
  }

  return *service_.get();
}

void TracingServiceController::RegisterClientOnUIThread(
    base::ProcessId pid,
    EnableTracingCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the service is currently running, immediately connect the new client.
  if (service_) {
    mojo::PendingRemote<tracing::mojom::TracedProcess> remote_process;
    callback.Run(remote_process.InitWithNewPipeAndPassReceiver());
    service_->AddClient(
        tracing::mojom::ClientInfo::New(pid, std::move(remote_process)));
  }

  clients_.emplace(pid, std::move(callback));
}

void TracingServiceController::RemoveClient(base::ProcessId pid) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TracingServiceController::RemoveClient,
                                  base::Unretained(this), pid));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  clients_.erase(pid);
}

tracing::mojom::TracingService& GetTracingService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return TracingServiceController::Get().GetService();
}

}  // namespace content
