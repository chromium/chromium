// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_HOST_H_
#define CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_HOST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom.h"

namespace content {

class CookieStoreManager;

// Stores the state associated with each CookieStore mojo connection.
//
// The bulk of the CookieStore implementation is in the CookieStoreManager
// class. Each StoragePartition has a single associated CookieStoreManager
// instance. By contrast, each CookieStore mojo connection has an associated
// CookieStoreHost instance, which stores the per-connection state.
//
// Instances of this class must be accessed exclusively on the IO thread,
// because they call into CookieStoreManager directly.
class CookieStoreHost : public blink::mojom::CookieStore {
 public:
  CookieStoreHost(CookieStoreManager* manager,
                  const blink::StorageKey& storage_key);

  CookieStoreHost(const CookieStoreHost&) = delete;
  CookieStoreHost& operator=(const CookieStoreHost&) = delete;

  ~CookieStoreHost() override;

  // content::mojom::CookieStore
  void AddSubscriptions(
      int64_t service_worker_registration_id,
      std::vector<blink::mojom::CookieChangeSubscriptionPtr> subscriptions,
      AddSubscriptionsCallback callback) override;
  void RemoveSubscriptions(
      int64_t service_worker_registration_id,
      std::vector<blink::mojom::CookieChangeSubscriptionPtr> subscriptions,
      RemoveSubscriptionsCallback callback) override;
  void GetSubscriptions(int64_t service_worker_registration_id,
                        GetSubscriptionsCallback callback) override;

 private:
  // The raw pointer is safe because CookieStoreManager owns this instance via a
  // mojo::UniqueReceiverSet.
  const raw_ptr<CookieStoreManager> manager_;

  const blink::StorageKey storage_key_;

  // Instances of this class are currently bound to the IO thread, because they
  // call ServiceWorkerContextWrapper methods that are restricted to the IO
  // thread. However, the class implementation itself is thread-friendly, so it
  // only checks that methods are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_STORE_COOKIE_STORE_HOST_H_
