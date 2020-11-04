// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_provider.h"

#include "base/macros.h"

namespace service_manager {

InterfaceProvider::InterfaceProvider(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : pending_receiver_(
          interface_provider_.BindNewPipeAndPassReceiver(task_runner)),
      task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

InterfaceProvider::InterfaceProvider(
    mojo::PendingRemote<mojom::InterfaceProvider> interface_provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : interface_provider_(std::move(interface_provider), task_runner),
      task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

InterfaceProvider::~InterfaceProvider() = default;

void InterfaceProvider::Close() {
  if (pending_receiver_)
    pending_receiver_.PassPipe().reset();
  interface_provider_.reset();
}

void InterfaceProvider::Bind(
    mojo::PendingRemote<mojom::InterfaceProvider> interface_provider) {
  DCHECK(pending_receiver_ || !interface_provider_);
  DCHECK(forward_callback_.is_null());
  if (pending_receiver_) {
    mojo::FusePipes(std::move(pending_receiver_),
                    std::move(interface_provider));
  } else {
    interface_provider_.Bind(std::move(interface_provider), task_runner_);
  }
}

void InterfaceProvider::Forward(const ForwardCallback& callback) {
  DCHECK(pending_receiver_);
  DCHECK(forward_callback_.is_null());
  interface_provider_.reset();
  pending_receiver_.PassPipe().reset();
  forward_callback_ = callback;
}

void InterfaceProvider::SetConnectionLostClosure(
    base::OnceClosure connection_lost_closure) {
  interface_provider_.set_disconnect_handler(
      std::move(connection_lost_closure));
}

base::WeakPtr<InterfaceProvider> InterfaceProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void InterfaceProvider::GetInterfaceByName(
    const std::string& name,
    mojo::ScopedMessagePipeHandle request_handle) {
  // Local binders can be registered via TestApi.
  auto it = binders_.find(name);
  if (it != binders_.end()) {
    it->second.Run(std::move(request_handle));
    return;
  }

  if (!forward_callback_.is_null()) {
    DCHECK(!interface_provider_.is_bound());
    forward_callback_.Run(name, std::move(request_handle));
  } else {
    DCHECK(interface_provider_.is_bound());
    interface_provider_->GetInterface(name, std::move(request_handle));
  }
}

bool InterfaceProvider::HasBinderForName(const std::string& name) const {
  return binders_.find(name) != binders_.end();
}

void InterfaceProvider::ClearBinderForName(const std::string& name) {
  binders_.erase(name);
}

void InterfaceProvider::ClearBinders() {
  binders_.clear();
}

}  // namespace service_manager
