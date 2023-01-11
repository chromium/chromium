// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/service_factory.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"

namespace mojo {

ServiceFactory::ServiceFactory() = default;

ServiceFactory::~ServiceFactory() = default;

bool ServiceFactory::CanRunService(
    const GenericPendingReceiver& receiver) const {
  DCHECK(receiver.is_valid());
  return base::Contains(constructors_, *receiver.interface_name());
}

bool ServiceFactory::RunService(GenericPendingReceiver receiver,
                                base::OnceClosure termination_callback) {
  DCHECK(receiver.is_valid());

  // We grab a weak handle to the receiver's message pipe first. If any function
  // accepts the receiver, we will tie its returned object's lifetime to the
  // connection state of that pipe.
  MessagePipeHandle pipe = receiver.pipe();

  auto it = constructors_.find(*receiver.interface_name());
  if (it == constructors_.end())
    return false;

  auto instance = it->second.Run(std::move(receiver));
  if (!instance)
    return false;

  auto disconnect_callback =
      base::BindOnce(&ServiceFactory::OnInstanceDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), instance.get());
  if (termination_callback) {
    disconnect_callback =
        std::move(disconnect_callback).Then(std::move(termination_callback));
  }
  instance->WatchPipe(pipe, std::move(disconnect_callback));
  instances_.insert(std::move(instance));
  return true;
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
  watcher_.Watch(pipe, MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 base::BindRepeating(&InstanceHolderBase::OnPipeSignaled,
                                     base::Unretained(this)));
}

void ServiceFactory::InstanceHolderBase::OnPipeSignaled(
    MojoResult result,
    const HandleSignalsState& state) {
  // We only care about the two conditions below. FAILED_PRECONDITION implies
  // that the peer was closed and all its sent messages have been read (and
  // dispatched) locally, while CANCELLED implies that the service pipe was
  // closed locally. In both cases, we run the callback which will delete
  // `this` and, ultimately, the service instance itself.
  if (result == MOJO_RESULT_FAILED_PRECONDITION ||
      result == MOJO_RESULT_CANCELLED) {
    watcher_.Cancel();
    DCHECK(disconnect_callback_);
    std::move(disconnect_callback_).Run();
  }
}

}  // namespace mojo
