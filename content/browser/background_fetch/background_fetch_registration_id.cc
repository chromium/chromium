// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_registration_id.h"

#include <tuple>

#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId()
    : service_worker_registration_id_(
          blink::mojom::kInvalidServiceWorkerRegistrationId) {}

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    const std::string& unique_id)
    : service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      developer_id_(developer_id),
      unique_id_(unique_id) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);
  DCHECK(!unique_id_.empty());
}

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId(
    const BackgroundFetchRegistrationId& other) = default;

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId(
    BackgroundFetchRegistrationId&& other) = default;

BackgroundFetchRegistrationId& BackgroundFetchRegistrationId::operator=(
    const BackgroundFetchRegistrationId& other) = default;

BackgroundFetchRegistrationId& BackgroundFetchRegistrationId::operator=(
    BackgroundFetchRegistrationId&& other) = default;

BackgroundFetchRegistrationId::~BackgroundFetchRegistrationId() = default;

bool BackgroundFetchRegistrationId::operator==(
    const BackgroundFetchRegistrationId& other) const {
  return unique_id_ == other.unique_id_;
}

bool BackgroundFetchRegistrationId::operator!=(
    const BackgroundFetchRegistrationId& other) const {
  return unique_id_ != other.unique_id_;
}

bool BackgroundFetchRegistrationId::operator<(
    const BackgroundFetchRegistrationId& other) const {
  return unique_id_ < other.unique_id_;
}

bool BackgroundFetchRegistrationId::is_null() const {
  return unique_id_.empty();
}

}  // namespace content
