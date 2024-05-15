// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/service_worker/service_worker_router_evaluator.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"

namespace content {

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    mojo::PendingRemote<blink::mojom::CacheStorage> remote_cache_storage,
    const std::string& client_id,
    blink::mojom::ServiceWorkerFetchHandlerBypassOption
        fetch_handler_bypass_option,
    std::optional<blink::ServiceWorkerRouterRules> router_rules,
    blink::EmbeddedWorkerStatus initial_running_status,
    mojo::PendingReceiver<blink::mojom::ServiceWorkerRunningStatusCallback>
        running_status_receiver)
    : client_id_(client_id),
      fetch_handler_bypass_option_(fetch_handler_bypass_option),
      running_status_(initial_running_status),
      running_status_receiver_(this) {
  container_host_.Bind(std::move(remote_container_host));
  container_host_.set_disconnect_handler(base::BindOnce(
      &ControllerServiceWorkerConnector::OnContainerHostConnectionClosed,
      base::Unretained(this)));
  if (router_rules) {
    router_evaluator_ =
        std::make_unique<content::ServiceWorkerRouterEvaluator>(*router_rules);
    CHECK(router_evaluator_->IsValid());
    if (remote_cache_storage) {
      cache_storage_.Bind(std::move(remote_cache_storage));
    }
    if (running_status_receiver) {
      CHECK(router_evaluator_->need_running_status());
      running_status_receiver_.Bind(std::move(running_status_receiver));
    }
  }
  SetControllerServiceWorker(std::move(remote_controller));
}

blink::mojom::ControllerServiceWorker*
ControllerServiceWorkerConnector::GetControllerServiceWorker(
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  switch (state_) {
    case State::kDisconnected: {
      DCHECK(!controller_service_worker_);
      DCHECK(container_host_);
      mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
          remote_controller;

      container_host_->EnsureControllerServiceWorker(
          remote_controller.InitWithNewPipeAndPassReceiver(), purpose);

      SetControllerServiceWorker(std::move(remote_controller));
      return controller_service_worker_.get();
    }
    case State::kConnected:
      DCHECK(controller_service_worker_.is_bound());
      return controller_service_worker_.get();
    case State::kNoController:
      DCHECK(!controller_service_worker_);
      return nullptr;
    case State::kNoContainerHost:
      DCHECK(!controller_service_worker_);
      DCHECK(!container_host_);
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void ControllerServiceWorkerConnector::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ControllerServiceWorkerConnector::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ControllerServiceWorkerConnector::OnContainerHostConnectionClosed() {
  state_ = State::kNoContainerHost;
  container_host_.reset();
  controller_service_worker_.reset();
}

void ControllerServiceWorkerConnector::OnControllerConnectionClosed() {
  DCHECK_EQ(State::kConnected, state_);
  state_ = State::kDisconnected;
  controller_service_worker_.reset();
  for (auto& observer : observer_list_)
    observer.OnConnectionClosed();
}

void ControllerServiceWorkerConnector::EnsureFileAccess(
    const std::vector<base::FilePath>& file_paths,
    base::OnceClosure callback) {
  container_host_->EnsureFileAccess(file_paths, std::move(callback));
}

void ControllerServiceWorkerConnector::AddBinding(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorkerConnector>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ControllerServiceWorkerConnector::UpdateController(
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller) {
  if (state_ == State::kNoContainerHost)
    return;
  SetControllerServiceWorker(std::move(controller));
  if (!controller_service_worker_)
    state_ = State::kNoController;
}

void ControllerServiceWorkerConnector::SetControllerServiceWorker(
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller) {
  controller_service_worker_.reset();
  if (!controller)
    return;
  controller_service_worker_.Bind(std::move(controller));
  if (controller_service_worker_) {
    controller_service_worker_.set_disconnect_handler(base::BindOnce(
        &ControllerServiceWorkerConnector::OnControllerConnectionClosed,
        base::Unretained(this)));
    state_ = State::kConnected;
  }
}

blink::EmbeddedWorkerStatus
ControllerServiceWorkerConnector::GetRecentRunningStatus() {
  return running_status_;
}

void ControllerServiceWorkerConnector::OnStatusChanged(
    blink::EmbeddedWorkerStatus running_status) {
  // A callback to update `running_status_` is set only if
  // ServiceWorkerRouterEvaluator requires the running status.
  // Otherwise, `running_status_` may not be a meaningful value.
  CHECK(router_evaluator_->need_running_status());
  running_status_ = running_status;
}

void ControllerServiceWorkerConnector::CallCacheStorageMatch(
    std::optional<std::string> cache_name,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheStorage::MatchCallback callback) {
  if (!cache_storage_ || !cache_storage_.is_bound()) {
    std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
        blink::mojom::CacheStorageError::kErrorStorage));
    return;
  }
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  auto options = blink::mojom::MultiCacheQueryOptions::New();
  options->query_options = blink::mojom::CacheQueryOptions::New();
  if (cache_name) {
    options->cache_name = base::UTF8ToUTF16(*cache_name);
  }
  cache_storage_->Match(std::move(request), std::move(options),
                        /*in_related_fetch_event=*/false,
                        /*in_range_fetch_event=*/false, trace_id,
                        std::move(callback));
}

ControllerServiceWorkerConnector::~ControllerServiceWorkerConnector() = default;

}  // namespace content
