// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

#include <tuple>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"

namespace blink {

void BrowserInterfaceBrokerProxy::Bind(
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  broker_ = mojo::Remote<blink::mojom::BrowserInterfaceBroker>(
      std::move(broker), std::move(task_runner));
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
BrowserInterfaceBrokerProxy::Reset(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  broker_.reset();
  return broker_.BindNewPipeAndPassReceiver(std::move(task_runner));
}

void BrowserInterfaceBrokerProxy::GetInterface(
    mojo::GenericPendingReceiver receiver) const {
  // Local binders can be registered via SetBinderForTesting.
  DCHECK(receiver.interface_name());
  auto it = binder_map_for_testing_.find(receiver.interface_name().value());
  if (it != binder_map_for_testing_.end()) {
    it->second.Run(receiver.PassPipe());
    return;
  }

  broker_->GetInterface(std::move(receiver));
}

void BrowserInterfaceBrokerProxy::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle pipe) const {
  GetInterface(mojo::GenericPendingReceiver(name, std::move(pipe)));
}

bool BrowserInterfaceBrokerProxy::is_bound() const {
  return broker_.is_bound();
}

bool BrowserInterfaceBrokerProxy::SetBinderForTesting(
    const std::string& name,
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder) const {
  if (!binder) {
    binder_map_for_testing_.erase(name);
    return true;
  }

  auto result = binder_map_for_testing_.emplace(name, std::move(binder));
  return result.second;
}

BrowserInterfaceBrokerProxy& GetEmptyBrowserInterfaceBroker() {
  static base::SequenceLocalStorageSlot<BrowserInterfaceBrokerProxy> proxy_slot;
  if (!proxy_slot.GetValuePointer()) {
    auto& proxy = proxy_slot.GetOrCreateValue();
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote;
    std::ignore = remote.InitWithNewPipeAndPassReceiver();
    proxy.Bind(std::move(remote),
               base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  return proxy_slot.GetOrCreateValue();
}

}  // namespace blink
