// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_shared_storage_host.h"

#include "components/services/storage/shared_storage/shared_storage_manager.h"

namespace content {

struct AuctionSharedStorageHost::ReceiverContext {
  url::Origin worklet_origin;
};

AuctionSharedStorageHost::AuctionSharedStorageHost(
    storage::SharedStorageManager* shared_storage_manager)
    : shared_storage_manager_(shared_storage_manager) {
  DCHECK(shared_storage_manager_);
}

AuctionSharedStorageHost::~AuctionSharedStorageHost() = default;

void AuctionSharedStorageHost::BindNewReceiver(
    const url::Origin& worklet_origin,
    mojo::PendingReceiver<auction_worklet::mojom::AuctionSharedStorageHost>
        receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    ReceiverContext{.worklet_origin = worklet_origin});
}

void AuctionSharedStorageHost::Set(const std::u16string& key,
                                   const std::u16string& value,
                                   bool ignore_if_present) {
  storage::SharedStorageManager::SetBehavior set_behavior =
      ignore_if_present
          ? storage::SharedStorageManager::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageManager::SetBehavior::kDefault;

  shared_storage_manager_->Set(receiver_set_.current_context().worklet_origin,
                               key, value, base::DoNothing(), set_behavior);
}

void AuctionSharedStorageHost::Append(const std::u16string& key,
                                      const std::u16string& value) {
  shared_storage_manager_->Append(
      receiver_set_.current_context().worklet_origin, key, value,
      base::DoNothing());
}

void AuctionSharedStorageHost::Delete(const std::u16string& key) {
  shared_storage_manager_->Delete(
      receiver_set_.current_context().worklet_origin, key, base::DoNothing());
}

void AuctionSharedStorageHost::Clear() {
  shared_storage_manager_->Clear(receiver_set_.current_context().worklet_origin,
                                 base::DoNothing());
}

}  // namespace content
