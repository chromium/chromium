// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_STORAGE_ACCESS_STATUS_CACHE_H_
#define NET_URL_REQUEST_STORAGE_ACCESS_STATUS_CACHE_H_

#include <optional>
#include <variant>

#include "base/types/optional_util.h"
#include "net/cookies/cookie_util.h"

namespace net {

// Holds the StorageAccessStatus of the request.
// TODO(https://crbug.com/366284840): move this out of //net together with the
// `URLRequest::storage_access_status_`.
class StorageAccessStatusCache {
 public:
  StorageAccessStatusCache() = default;

  explicit StorageAccessStatusCache(
      std::optional<net::cookie_util::StorageAccessStatus>
          storage_access_status) {
    if (storage_access_status.has_value()) {
      state_ = storage_access_status.value();
    } else {
      state_ = FirstParty{};
    }
  }

  friend bool operator==(const StorageAccessStatusCache& status,
                         cookie_util::StorageAccessStatus status_enum) {
    return status.GetStatusForThirdPartyContext() == status_enum;
  }

  // Returns the cached storage access status as an optional. Nullopt if the
  // state is `FirstParty`.
  std::optional<net::cookie_util::StorageAccessStatus>
  GetStatusForThirdPartyContext() const {
    CHECK(IsSet());
    return base::OptionalFromPtr(
        std::get_if<net::cookie_util::StorageAccessStatus>(&state_));
  }

  bool IsSet() const { return !std::holds_alternative<Unset>(state_); }

  void Reset() { state_ = Unset{}; }

 private:
  // `state_` variant used when the storage access status is not set.
  struct Unset {};

  // `state_` variant used when the storage access status is not applicable
  // because the request is first-party.
  struct FirstParty {};

  std::variant<Unset, FirstParty, net::cookie_util::StorageAccessStatus> state_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_STORAGE_ACCESS_STATUS_CACHE_H_
