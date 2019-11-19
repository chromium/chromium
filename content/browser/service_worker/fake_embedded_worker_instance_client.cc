// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"

namespace content {

FakeEmbeddedWorkerInstanceClient::FakeEmbeddedWorkerInstanceClient(
    EmbeddedWorkerTestHelper* helper)
    : helper_(helper) {}

FakeEmbeddedWorkerInstanceClient::~FakeEmbeddedWorkerInstanceClient() = default;

base::WeakPtr<FakeEmbeddedWorkerInstanceClient>
FakeEmbeddedWorkerInstanceClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FakeEmbeddedWorkerInstanceClient::Bind(
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
        receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&FakeEmbeddedWorkerInstanceClient::OnConnectionError,
                     base::Unretained(this)));

  if (quit_closure_for_bind_)
    std::move(quit_closure_for_bind_).Run();
}

void FakeEmbeddedWorkerInstanceClient::RunUntilBound() {
  if (receiver_.is_bound())
    return;
  base::RunLoop loop;
  quit_closure_for_bind_ = loop.QuitClosure();
  loop.Run();
}

void FakeEmbeddedWorkerInstanceClient::Disconnect() {
  receiver_.reset();
  OnConnectionError();
}

void FakeEmbeddedWorkerInstanceClient::StartWorker(
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  host_.Bind(std::move(params->instance_host));
  start_params_ = std::move(params);

  helper_->OnServiceWorkerReceiver(
      std::move(start_params_->service_worker_receiver));

  // Create message pipes. We may need to keep |devtools_agent_receiver| and
  // |devtools_agent_host_remote| if we want not to invoke
  // connection error handlers.

  mojo::PendingRemote<blink::mojom::DevToolsAgent> devtools_agent_remote;
  mojo::PendingReceiver<blink::mojom::DevToolsAgent> devtools_agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();

  mojo::Remote<blink::mojom::DevToolsAgentHost> devtools_agent_host_remote;
  host_->OnReadyForInspection(
      std::move(devtools_agent_remote),
      devtools_agent_host_remote.BindNewPipeAndPassReceiver());

  if (start_params_->is_installed) {
    EvaluateScript();
    return;
  }

  // In production, new service workers would request their main script here,
  // which causes the browser to write the script response in service worker
  // storage. We do that manually here.
  //
  // TODO(falken): For new workers, this should use
  // |script_loader_factory_remote| from |start_params_->provider_info|
  // to request the script and the browser process should be able to mock it.
  // For installed workers, the map should already be populated.
  ServiceWorkerVersion* version = helper_->context()->GetLiveVersion(
      start_params_->service_worker_version_id);
  if (version && version->status() == ServiceWorkerVersion::REDUNDANT) {
    // This can happen if ForceDelete() was called on the registration. Early
    // return because otherwise PopulateScriptCacheMap will DCHECK. If we mocked
    // things as per the TODO, the script load would fail and we don't need to
    // special case this.
    return;
  }
  helper_->PopulateScriptCacheMap(
      start_params_->service_worker_version_id,
      base::BindOnce(
          &FakeEmbeddedWorkerInstanceClient::DidPopulateScriptCacheMap,
          GetWeakPtr()));
}

void FakeEmbeddedWorkerInstanceClient::StopWorker() {
  host_->OnStopped();

  // Destroys |this|. This matches the production implementation, which
  // calls OnStopped() from the worker thread and then posts task
  // to the EmbeddedWorkerInstanceClient to have it self-destruct.
  OnConnectionError();
}

void FakeEmbeddedWorkerInstanceClient::ResumeAfterDownload() {
  EvaluateScript();
}

void FakeEmbeddedWorkerInstanceClient::DidPopulateScriptCacheMap() {
  host_->OnScriptLoaded();
  if (start_params_->pause_after_download) {
    // We continue when ResumeAfterDownload() is called.
    return;
  }
  EvaluateScript();
}

void FakeEmbeddedWorkerInstanceClient::OnConnectionError() {
  // Destroys |this|.
  helper_->RemoveInstanceClient(this);
}

void FakeEmbeddedWorkerInstanceClient::EvaluateScript() {
  host_->OnScriptEvaluationStart();
  host_->OnStarted(blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
                   helper_->GetNextThreadId(),
                   blink::mojom::EmbeddedWorkerStartTiming::New());
}

DelayedFakeEmbeddedWorkerInstanceClient::
    DelayedFakeEmbeddedWorkerInstanceClient(EmbeddedWorkerTestHelper* helper)
    : FakeEmbeddedWorkerInstanceClient(helper) {}

DelayedFakeEmbeddedWorkerInstanceClient::
    ~DelayedFakeEmbeddedWorkerInstanceClient() = default;

void DelayedFakeEmbeddedWorkerInstanceClient::UnblockStartWorker() {
  switch (start_state_) {
    case State::kWontBlock:
    case State::kCompleted:
      break;
    case State::kWillBlock:
      start_state_ = State::kWontBlock;
      break;
    case State::kBlocked:
      FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(start_params_));
      start_state_ = State::kCompleted;
      break;
  }
}

void DelayedFakeEmbeddedWorkerInstanceClient::UnblockStopWorker() {
  switch (stop_state_) {
    case State::kWontBlock:
    case State::kCompleted:
      break;
    case State::kWillBlock:
      stop_state_ = State::kWontBlock;
      break;
    case State::kBlocked:
      // Destroys |this|.
      CompleteStopWorker();
      return;
  }
}

void DelayedFakeEmbeddedWorkerInstanceClient::RunUntilStartWorker() {
  switch (start_state_) {
    case State::kBlocked:
    case State::kCompleted:
      return;
    case State::kWontBlock:
    case State::kWillBlock:
      break;
  }
  base::RunLoop loop;
  quit_closure_for_start_worker_ = loop.QuitClosure();
  loop.Run();
}

void DelayedFakeEmbeddedWorkerInstanceClient::RunUntilStopWorker() {
  switch (stop_state_) {
    case State::kBlocked:
    case State::kCompleted:
      return;
    case State::kWontBlock:
    case State::kWillBlock:
      break;
  }
  base::RunLoop loop;
  quit_closure_for_stop_worker_ = loop.QuitClosure();
  loop.Run();
}

void DelayedFakeEmbeddedWorkerInstanceClient::StartWorker(
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  switch (start_state_) {
    case State::kWontBlock:
      FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(params));
      start_state_ = State::kCompleted;
      break;
    case State::kWillBlock:
      start_params_ = std::move(params);
      start_state_ = State::kBlocked;
      break;
    case State::kCompleted:
    case State::kBlocked:
      NOTREACHED();
      break;
  }
  if (quit_closure_for_start_worker_)
    std::move(quit_closure_for_start_worker_).Run();
}

void DelayedFakeEmbeddedWorkerInstanceClient::StopWorker() {
  switch (stop_state_) {
    case State::kWontBlock:
      // Run the closure before destroying.
      if (quit_closure_for_stop_worker_)
        std::move(quit_closure_for_stop_worker_).Run();
      // Destroys |this|.
      CompleteStopWorker();
      return;
    case State::kWillBlock:
    case State::kBlocked:
      stop_state_ = State::kBlocked;
      break;
    case State::kCompleted:
      NOTREACHED();
      break;
  }
  if (quit_closure_for_stop_worker_)
    std::move(quit_closure_for_stop_worker_).Run();
}

void DelayedFakeEmbeddedWorkerInstanceClient::CompleteStopWorker() {
  if (!host()) {
    // host() might not be bound if start never was called or is blocked.
    DCHECK(start_params_);
    host().Bind(std::move(start_params_->instance_host));
  }
  // Destroys |this|.
  FakeEmbeddedWorkerInstanceClient::StopWorker();
}

}  // namespace content
