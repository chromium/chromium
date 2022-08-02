// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace network {
struct CrossOriginEmbedderPolicy;
}

namespace storage {
struct BucketLocator;
}

namespace content {

class CacheStorageContextImpl;

// Handles Cache Storage related messages sent to the browser process from
// child processes. One host instance exists per child process. Each host
// is bound to the cache_storage scheduler sequence and may not be accessed
// from other sequences.
class CacheStorageDispatcherHost {
 public:
  explicit CacheStorageDispatcherHost(CacheStorageContextImpl* context);

  CacheStorageDispatcherHost(const CacheStorageDispatcherHost&) = delete;
  CacheStorageDispatcherHost& operator=(const CacheStorageDispatcherHost&) =
      delete;

  ~CacheStorageDispatcherHost();

  // Binds the CacheStorage Mojo receiver to this instance.
  // NOTE: The same CacheStorageDispatcherHost instance may be bound to
  // different clients on different origins. Each context is kept on
  // BindingSet's context. This guarantees that the browser process uses the
  // origin of the client known at the binding time, instead of relying on the
  // client to provide its origin at every method call.
  void AddReceiver(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      const blink::StorageKey& storage_key,
      const absl::optional<storage::BucketLocator>& bucket,
      storage::mojom::CacheStorageOwner owner,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

 private:
  class CacheStorageImpl;
  class CacheImpl;
  friend class CacheImpl;

  void AddCacheReceiver(
      std::unique_ptr<CacheImpl> cache_impl,
      mojo::PendingAssociatedReceiver<blink::mojom::CacheStorageCache>
          receiver);
  CacheStorageHandle OpenCacheStorage(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner);

  // `this` is owned by `context_`.
  const raw_ptr<CacheStorageContextImpl> context_;

  mojo::UniqueReceiverSet<blink::mojom::CacheStorage> receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::CacheStorageCache>
      cache_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
