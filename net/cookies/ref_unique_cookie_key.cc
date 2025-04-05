// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/ref_unique_cookie_key.h"

#include <compare>
#include <optional>
#include <string>
#include <utility>

#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

class CookieBase;

// static
RefUniqueCookieKey RefUniqueCookieKey::Host(
    base::PassKey<CookieBase>,
    const std::optional<CookiePartitionKey>& partition_key,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    const std::optional<CookieSourceScheme>& source_scheme,
    const std::optional<int>& source_port) {
  return RefUniqueCookieKey(KeyType::kHost, partition_key, name, domain, path,
                            source_scheme, source_port);
}

// static
RefUniqueCookieKey RefUniqueCookieKey::Domain(
    base::PassKey<CookieBase>,
    const std::optional<CookiePartitionKey>& partition_key,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    const std::optional<CookieSourceScheme>& source_scheme) {
  return RefUniqueCookieKey(KeyType::kDomain, partition_key, name, domain, path,
                            source_scheme, /*port=*/std::nullopt);
}

RefUniqueCookieKey::RefUniqueCookieKey(RefUniqueCookieKey&& other) = default;

RefUniqueCookieKey::~RefUniqueCookieKey() = default;

RefUniqueCookieKey::RefUniqueCookieKey(
    KeyType key_type,
    const std::optional<CookiePartitionKey>& partition_key,
    const std::string& name,
    const std::string& domain,
    const std::string& path,
    const std::optional<CookieSourceScheme>& source_scheme,
    const std::optional<int>& port)
    : key_type_(key_type),
      partition_key_(std::move(partition_key)),
      name_(std::move(name)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      source_scheme_(source_scheme),
      port_(port) {}

}  // namespace net
