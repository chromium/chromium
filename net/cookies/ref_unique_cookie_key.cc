// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/ref_unique_cookie_key.h"

#include <compare>
#include <optional>
#include <string_view>

#include "base/types/optional_ref.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

class CookieBase;

// static
RefUniqueCookieKey RefUniqueCookieKey::Host(
    base::PassKey<CookieBase>,
    base::optional_ref<const CookiePartitionKey> partition_key,
    std::string_view name,
    std::string_view domain,
    std::string_view path,
    std::optional<CookieSourceScheme> source_scheme,
    std::optional<int> source_port) {
  return RefUniqueCookieKey(KeyType::kHost, partition_key, name, domain, path,
                            source_scheme, source_port);
}

// static
RefUniqueCookieKey RefUniqueCookieKey::Domain(
    base::PassKey<CookieBase>,
    base::optional_ref<const CookiePartitionKey> partition_key,
    std::string_view name,
    std::string_view domain,
    std::string_view path,
    std::optional<CookieSourceScheme> source_scheme) {
  return RefUniqueCookieKey(KeyType::kDomain, partition_key, name, domain, path,
                            source_scheme, /*port=*/std::nullopt);
}

RefUniqueCookieKey::RefUniqueCookieKey(RefUniqueCookieKey&&) = default;

RefUniqueCookieKey::~RefUniqueCookieKey() = default;

RefUniqueCookieKey::RefUniqueCookieKey(
    KeyType key_type,
    base::optional_ref<const CookiePartitionKey> partition_key,
    std::string_view name,
    std::string_view domain,
    std::string_view path,
    std::optional<CookieSourceScheme> source_scheme,
    std::optional<int> port)
    : key_type_(key_type),
      partition_key_(partition_key),
      name_(name),
      domain_(domain),
      path_(path),
      source_scheme_(source_scheme),
      port_(port) {}

}  // namespace net
