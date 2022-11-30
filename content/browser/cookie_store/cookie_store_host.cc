// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_store_host.h"

#include <utility>

#include "content/browser/cookie_store/cookie_store_manager.h"
#include "mojo/public/cpp/bindings/message.h"
#include "url/origin.h"

namespace content {

CookieStoreHost::CookieStoreHost(CookieStoreManager* manager,
                                 const url::Origin& origin)
    : manager_(manager), origin_(origin) {}

CookieStoreHost::~CookieStoreHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CookieStoreHost::AddSubscriptions(
    int64_t service_worker_registration_id,
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> subscriptions,
    AddSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->AddSubscriptions(
      service_worker_registration_id, origin_, std::move(subscriptions),
      mojo::GetBadMessageCallback(), std::move(callback));
}

void CookieStoreHost::RemoveSubscriptions(
    int64_t service_worker_registration_id,
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> subscriptions,
    RemoveSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->RemoveSubscriptions(
      service_worker_registration_id, origin_, std::move(subscriptions),
      mojo::GetBadMessageCallback(), std::move(callback));
}

void CookieStoreHost::GetSubscriptions(int64_t service_worker_registration_id,
                                       GetSubscriptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->GetSubscriptions(service_worker_registration_id, origin_,
                             mojo::GetBadMessageCallback(),
                             std::move(callback));
}

}  // namespace content
