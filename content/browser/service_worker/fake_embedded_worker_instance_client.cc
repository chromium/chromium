// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace content {

class FakeServiceWorkerInstalledScriptsManager
    : public blink::mojom::ServiceWorkerInstalledScriptsManager {
 public:
  explicit FakeServiceWorkerInstalledScriptsManager(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerInstalledScriptsManager>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  blink::mojom::ServiceWorkerScriptInfoPtr WaitForTransferInstalledScript() {
    if (!script_info_) {
      base::RunLoop loop;
      quit_closure_ = loop.QuitClosure();
      loop.Run();
      DCHECK(script_info_);
    }
    return std::move(script_info_);
  }

 private:
  void TransferInstalledScript(
      blink::mojom::ServiceWorkerScriptInfoPtr script_info) override {
    script_info_ = std::move(script_info);
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
  blink::mojom::ServiceWorkerScriptInfoPtr script_info_;

  mojo::Receiver<blink::mojom::ServiceWorkerInstalledScriptsManager> receiver_;
};

// URLLoaderClient lives until OnComplete() is called.
class FakeEmbeddedWorkerInstanceClient::LoaderClient final
    : public network::mojom::URLLoaderClient {
 public:
  LoaderClient(mojo::PendingReceiver<network::mojom::URLLoaderClient> receiver,
               base::OnceClosure callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {}
  ~LoaderClient() override = default;

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {}
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kFakeEmbeddedWorkerInstanceClient);
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    auto callback = std::move(callback_);
    std::move(callback).Run();
    // Do not add code after that, the object is deleted.
  }

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_;
  base::OnceClosure callback_;
};

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
      base::BindOnce(&FakeEmbeddedWorkerInstanceClient::CallOnConnectionError,
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

blink::mojom::ServiceWorkerScriptInfoPtr
FakeEmbeddedWorkerInstanceClient::WaitForTransferInstalledScript() {
  DCHECK(installed_scripts_manager_);
  return installed_scripts_manager_->WaitForTransferInstalledScript();
}

void FakeEmbeddedWorkerInstanceClient::Disconnect() {
  receiver_.reset();
  CallOnConnectionError();
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
    installed_scripts_manager_ =
        std::make_unique<FakeServiceWorkerInstalledScriptsManager>(
            std::move(start_params_->installed_scripts_info->manager_receiver));
    EvaluateScript();
    return;
  }

  // In production, new service workers would request their main script here,
  // which causes the browser to write the script response in service worker
  // storage. We do that manually here.
  if (start_params_->main_script_load_params) {
    // Wait until OnComplete() is called so that the script is stored in the
    // storage and the script cache map is populated by
    // ServiceWorkerNewScriptLoader.
    main_script_loader_client_ = std::make_unique<LoaderClient>(
        std::move(start_params_->main_script_load_params
                      ->url_loader_client_endpoints->url_loader_client),
        base::BindOnce(
            &FakeEmbeddedWorkerInstanceClient::DidPopulateScriptCacheMap,
            GetWeakPtr()));
  } else {
    // For installed workers, the map should already be populated.
    //
    // TODO(falken): For new workers, this should use
    // |script_loader_factory_remote| from |start_params_->provider_info|
    // to request the script and the browser process should be able to mock it.
    ServiceWorkerVersion* version = helper_->context()->GetLiveVersion(
        start_params_->service_worker_version_id);
    if (version && version->status() == ServiceWorkerVersion::REDUNDANT) {
      // This can happen if ForceDelete() was called on the registration. Early
      // return because otherwise PopulateScriptCacheMap will DCHECK. If we
      // mocked things as per the TODO, the script load would fail and we don't
      // need to special case this.
      return;
    }
    helper_->PopulateScriptCacheMap(
        start_params_->service_worker_version_id,
        base::BindOnce(
            &FakeEmbeddedWorkerInstanceClient::DidPopulateScriptCacheMap,
            GetWeakPtr()));
  }
}

void FakeEmbeddedWorkerInstanceClient::StopWorker() {
  host_->OnStopped();

  // Destroys |this|. This matches the production implementation, which
  // calls OnStopped() from the worker thread and then posts task
  // to the EmbeddedWorkerInstanceClient to have it self-destruct.
  CallOnConnectionError();
}

void FakeEmbeddedWorkerInstanceClient::DidPopulateScriptCacheMap() {
  main_script_loader_client_.reset();
  host_->OnScriptLoaded();
  EvaluateScript();
}

void FakeEmbeddedWorkerInstanceClient::OnConnectionError() {
  // Destroys |this|.
  helper_->RemoveInstanceClient(this);
}

void FakeEmbeddedWorkerInstanceClient::CallOnConnectionError() {
  // Call OnConnectionError(), which subclasses can override.
  OnConnectionError();
}

void FakeEmbeddedWorkerInstanceClient::EvaluateScript() {
  host_->OnScriptEvaluationStart();
  host_->OnStarted(blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
                   blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable,
                   /*has_hid_event_handlers=*/false,
                   /*has_usb_event_handlers=*/false, helper_->GetNextThreadId(),
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
