// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace content {

ControllerServiceWorkerConnector::ControllerServiceWorkerConnector(
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    const std::string& client_id)
    : client_id_(client_id) {
  container_host_.Bind(std::move(remote_container_host));
  container_host_.set_disconnect_handler(base::BindOnce(
      &ControllerServiceWorkerConnector::OnContainerHostConnectionClosed,
      base::Unretained(this)));
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
  NOTREACHED();
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

ControllerServiceWorkerConnector::~ControllerServiceWorkerConnector() = default;

}  // namespace content
