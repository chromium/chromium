// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/service_factory.h"

#include "base/bind.h"

namespace mojo {

ServiceFactory::~ServiceFactory() = default;

bool ServiceFactory::MaybeRunService(mojo::GenericPendingReceiver* receiver) {
  DCHECK(receiver->is_valid());

  // We grab a weak handle to the receiver's message pipe first. If any function
  // accepts the receiver, we will tie its returned object's lifetime to the
  // connection state of that pipe.
  MessagePipeHandle pipe = receiver->pipe();

  for (const auto& callback : callbacks_) {
    if (auto instance = callback.Run(receiver)) {
      DCHECK(!receiver->is_valid());
      instance->WatchPipe(
          pipe, base::BindOnce(&ServiceFactory::OnInstanceDisconnected,
                               weak_ptr_factory_.GetWeakPtr(), instance.get()));
      instances_.insert(std::move(instance));
      return true;
    }

    DCHECK(receiver->is_valid());
  }

  return false;
}

void ServiceFactory::OnInstanceDisconnected(InstanceHolderBase* instance) {
  instances_.erase(instance);
}

ServiceFactory::InstanceHolderBase::InstanceHolderBase()
    : watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {}

ServiceFactory::InstanceHolderBase::~InstanceHolderBase() = default;

void ServiceFactory::InstanceHolderBase::WatchPipe(
    MessagePipeHandle pipe,
    base::OnceClosure disconnect_callback) {
  DCHECK(!disconnect_callback_);
  disconnect_callback_ = std::move(disconnect_callback);
  watcher_.Watch(pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 base::BindRepeating(&InstanceHolderBase::OnDisconnect,
                                     base::Unretained(this)));
}

void ServiceFactory::InstanceHolderBase::OnDisconnect(
    MojoResult result,
    const HandleSignalsState& state) {
  // It doesn't matter what the parameters are, since the only way the watcher
  // can signal is if the peer was closed or the local pipe handle was closed.
  // The callback always destroys |this| when run, so there's also no chance of
  // this method running more than once.
  DCHECK(disconnect_callback_);
  std::move(disconnect_callback_).Run();
}

}  // namespace mojo
