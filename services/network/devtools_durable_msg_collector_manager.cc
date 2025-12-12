// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector_manager.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace network {

DevtoolsDurableMessageCollectorManager::
    DevtoolsDurableMessageCollectorManager() = default;

DevtoolsDurableMessageCollectorManager::
    ~DevtoolsDurableMessageCollectorManager() = default;

void DevtoolsDurableMessageCollectorManager::AddCollector(
    mojo::PendingReceiver<network::mojom::DurableMessageCollector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DevtoolsDurableMessageCollector>(
                                  weak_factory_.GetWeakPtr()),
                              std::move(receiver));
}

void DevtoolsDurableMessageCollectorManager::OnCollectorCreated(
    DevtoolsDurableMessageCollector* collector) {
  managed_collectors_testing_.insert(collector);
}

void DevtoolsDurableMessageCollectorManager::OnCollectorDestroyed(
    DevtoolsDurableMessageCollector* collector) {
  // An O(n) scan is fine here, because the number of collectors is
  // expected to be small.
  for (auto it = profile_collectors_.begin();
       it != profile_collectors_.end();) {
    if (it->second == collector) {
      it = profile_collectors_.erase(it);
    } else {
      ++it;
    }
  }
  managed_collectors_testing_.erase(collector);
}

std::vector<DevtoolsDurableMessageCollector*>
DevtoolsDurableMessageCollectorManager::GetCollectorsEnabledForProfile(
    const base::UnguessableToken& profile_id) {
  std::vector<DevtoolsDurableMessageCollector*> collectors;
  auto range = profile_collectors_.equal_range(profile_id);
  for (auto it = range.first; it != range.second; ++it) {
    collectors.push_back(it->second);
  }
  return collectors;
}

void DevtoolsDurableMessageCollectorManager::EnableForProfile(
    const base::UnguessableToken& profile_id,
    DevtoolsDurableMessageCollector& collector) {
  profile_collectors_.insert({profile_id, &collector});
}

void DevtoolsDurableMessageCollectorManager::DisableForProfile(
    const base::UnguessableToken& profile_id,
    DevtoolsDurableMessageCollector& collector) {
  auto range = profile_collectors_.equal_range(profile_id);
  for (auto it = range.first; it != range.second;) {
    if (it->second == &collector) {
      it = profile_collectors_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace network
