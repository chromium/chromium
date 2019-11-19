// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace blink {

void BrowserInterfaceBrokerProxy::Bind(
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker) {
  broker_ =
      mojo::Remote<blink::mojom::BrowserInterfaceBroker>(std::move(broker));
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
BrowserInterfaceBrokerProxy::Reset() {
  broker_.reset();
  return broker_.BindNewPipeAndPassReceiver();
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

bool BrowserInterfaceBrokerProxy::SetBinderForTesting(
    const std::string& name,
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)> binder) {
  if (!binder) {
    binder_map_for_testing_.erase(name);
    return true;
  }

  auto result = binder_map_for_testing_.emplace(name, std::move(binder));
  return result.second;
}

BrowserInterfaceBrokerProxy& GetEmptyBrowserInterfaceBroker() {
  static BrowserInterfaceBrokerProxy* broker_proxy = []() {
    BrowserInterfaceBrokerProxy* broker_proxy =
        new BrowserInterfaceBrokerProxy();
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> remote;
    ignore_result(remote.InitWithNewPipeAndPassReceiver());
    broker_proxy->Bind(std::move(remote));
    return broker_proxy;
  }();

  return *broker_proxy;
}

}  // namespace blink
