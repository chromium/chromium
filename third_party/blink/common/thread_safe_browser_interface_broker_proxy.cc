// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

#include <string_view>

namespace blink {

ThreadSafeBrowserInterfaceBrokerProxy::ThreadSafeBrowserInterfaceBrokerProxy() =
    default;

ThreadSafeBrowserInterfaceBrokerProxy::
    ~ThreadSafeBrowserInterfaceBrokerProxy() = default;

void ThreadSafeBrowserInterfaceBrokerProxy::GetInterface(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(receiver.interface_name());

  base::ReleasableAutoLock lock(&binder_map_lock_);
  auto it = binder_map_for_testing_.find(receiver.interface_name().value());
  if (it != binder_map_for_testing_.end()) {
    auto binder = it->second;
    lock.Release();
    binder.Run(receiver.PassPipe());
    return;
  }

  GetInterfaceImpl(std::move(receiver));
}

bool ThreadSafeBrowserInterfaceBrokerProxy::SetBinderForTesting(
    std::string_view interface_name,
    Binder binder) {
  std::string name(interface_name);

  base::AutoLock lock(binder_map_lock_);
  if (!binder) {
    binder_map_for_testing_.erase(name);
    return true;
  }

  auto result = binder_map_for_testing_.emplace(name, std::move(binder));
  return result.second;
}

}  // namespace blink
