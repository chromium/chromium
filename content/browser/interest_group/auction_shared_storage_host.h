// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_SHARED_STORAGE_HOST_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_SHARED_STORAGE_HOST_H_

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace storage {
class SharedStorageManager;
}  // namespace storage

namespace content {

class RenderFrameHostImpl;

// Implements the mojo interface used by the auction worklets, to receive
// shared storage requests and then pass them on to `SharedStorageManager` to
// write to the database.
class CONTENT_EXPORT AuctionSharedStorageHost
    : public auction_worklet::mojom::AuctionSharedStorageHost {
 public:
  explicit AuctionSharedStorageHost(
      storage::SharedStorageManager* shared_storage_manager);

  AuctionSharedStorageHost(const AuctionSharedStorageHost&) = delete;
  AuctionSharedStorageHost& operator=(const AuctionSharedStorageHost&) = delete;
  ~AuctionSharedStorageHost() override;

  // Binds a new pending receiver for a worklet, allowing messages to be sent
  // and processed.
  void BindNewReceiver(
      RenderFrameHostImpl* auction_runner_rfh,
      const url::Origin& worklet_origin,
      mojo::PendingReceiver<auction_worklet::mojom::AuctionSharedStorageHost>
          receiver);

  // auction_worklet::mojom::AuctionSharedStorageHost:
  void Set(const std::u16string& key,
           const std::u16string& value,
           bool ignore_if_present,
           auction_worklet::mojom::AuctionWorkletFunction
               source_auction_worklet_function) override;
  void Append(const std::u16string& key,
              const std::u16string& value,
              auction_worklet::mojom::AuctionWorkletFunction
                  source_auction_worklet_function) override;
  void Delete(const std::u16string& key,
              auction_worklet::mojom::AuctionWorkletFunction
                  source_auction_worklet_function) override;
  void Clear(auction_worklet::mojom::AuctionWorkletFunction
                 source_auction_worklet_function) override;

 private:
  struct ReceiverContext;

  mojo::ReceiverSet<auction_worklet::mojom::AuctionSharedStorageHost,
                    ReceiverContext>
      receiver_set_;

  // `this` is owned by a `DocumentService`, and `shared_storage_manager_` is
  // owned by the `StoragePartitionImpl`. Thus, `shared_storage_manager_` must
  // outlive `this`.
  raw_ptr<storage::SharedStorageManager> shared_storage_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_SHARED_STORAGE_HOST_H_
