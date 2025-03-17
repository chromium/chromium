// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/unique_cookie_key.h"

#include <optional>
#include <string>
#include <utility>

#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

class CookieBase;

// static
UniqueCookieKey UniqueCookieKey::Strict(
    base::PassKey<CookieBase>,
    std::optional<CookiePartitionKey> partition_key,
    std::string name,
    std::string domain,
    std::string path,
    CookieSourceScheme source_scheme,
    int source_port) {
  return UniqueCookieKey(KeyType::kStrict, std::move(partition_key),
                         std::move(name), std::move(domain), std::move(path),
                         source_scheme, source_port);
}

// static
UniqueCookieKey UniqueCookieKey::Host(
    base::PassKey<CookieBase>,
    std::optional<CookiePartitionKey> partition_key,
    std::string name,
    std::string domain,
    std::string path,
    std::optional<CookieSourceScheme> source_scheme,
    std::optional<int> source_port) {
  return UniqueCookieKey(KeyType::kHost, std::move(partition_key),
                         std::move(name), std::move(domain), std::move(path),
                         source_scheme, source_port);
}

// static
UniqueCookieKey UniqueCookieKey::Domain(
    base::PassKey<CookieBase>,
    std::optional<CookiePartitionKey> partition_key,
    std::string name,
    std::string domain,
    std::string path,
    std::optional<CookieSourceScheme> source_scheme) {
  return UniqueCookieKey(KeyType::kDomain, std::move(partition_key),
                         std::move(name), std::move(domain), std::move(path),
                         source_scheme, /*port=*/std::nullopt);
}

// static
UniqueCookieKey UniqueCookieKey::Legacy(
    base::PassKey<CookieBase>,
    std::optional<CookiePartitionKey> partition_key,
    std::string name,
    std::string domain,
    std::string path) {
  return UniqueCookieKey(KeyType::kLegacy, std::move(partition_key),
                         std::move(name), std::move(domain), std::move(path),
                         /*source_scheme=*/std::nullopt,
                         /*port=*/std::nullopt);
}

UniqueCookieKey::UniqueCookieKey(UniqueCookieKey&& other) = default;
UniqueCookieKey::UniqueCookieKey(const UniqueCookieKey& other) = default;
UniqueCookieKey& UniqueCookieKey::operator=(UniqueCookieKey&& other) = default;
UniqueCookieKey& UniqueCookieKey::operator=(const UniqueCookieKey& other) =
    default;

UniqueCookieKey::~UniqueCookieKey() = default;

UniqueCookieKey::UniqueCookieKey(
    KeyType key_type,
    std::optional<CookiePartitionKey> partition_key,
    std::string name,
    std::string domain,
    std::string path,
    std::optional<CookieSourceScheme> source_scheme,
    std::optional<int> port)
    : key_type_(key_type),
      partition_key_(std::move(partition_key)),
      name_(std::move(name)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      source_scheme_(source_scheme),
      port_(port) {}

}  // namespace net
