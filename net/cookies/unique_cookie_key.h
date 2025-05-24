// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_UNIQUE_COOKIE_KEY_H_
#define NET_COOKIES_UNIQUE_COOKIE_KEY_H_

#include <compare>
#include <optional>
#include <string>

#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

class CookieBase;

class NET_EXPORT UniqueCookieKey {
 public:
  // Always populates the cookie's source scheme and source port.
  static UniqueCookieKey Strict(base::PassKey<CookieBase>,
                                std::optional<CookiePartitionKey> partition_key,
                                std::string name,
                                std::string domain,
                                std::string path,
                                CookieSourceScheme source_scheme,
                                int source_port);

  // Conditionally populates the source scheme and source port depending on the
  // state of their associated feature.
  static UniqueCookieKey Host(base::PassKey<CookieBase>,
                              std::optional<CookiePartitionKey> partition_key,
                              std::string name,
                              std::string domain,
                              std::string path,
                              std::optional<CookieSourceScheme> source_scheme,
                              std::optional<int> source_port);

  // Same as Host but for use with Domain cookies, which do not
  // consider the source_port.
  static UniqueCookieKey Domain(
      base::PassKey<CookieBase>,
      std::optional<CookiePartitionKey> partition_key,
      std::string name,
      std::string domain,
      std::string path,
      std::optional<CookieSourceScheme> source_scheme);

  // Same as Host but for use with Legacy Scoped cookies, which do
  // not consider the source_port or source_scheme.
  static UniqueCookieKey Legacy(base::PassKey<CookieBase>,
                                std::optional<CookiePartitionKey> partition_key,
                                std::string name,
                                std::string domain,
                                std::string path);

  UniqueCookieKey(UniqueCookieKey&& other);
  UniqueCookieKey(const UniqueCookieKey& other);
  UniqueCookieKey& operator=(UniqueCookieKey&& other);
  UniqueCookieKey& operator=(const UniqueCookieKey& other);

  ~UniqueCookieKey();

  friend bool operator==(const UniqueCookieKey& left,
                         const UniqueCookieKey& right) = default;
  friend auto operator<=>(const UniqueCookieKey& left,
                          const UniqueCookieKey& right) = default;

  const std::string& name() const { return name_; }
  const std::string& domain() const { return domain_; }
  const std::string& path() const { return path_; }
  std::optional<CookieSourceScheme> source_scheme() const {
    return source_scheme_;
  }
  std::optional<int> port() const { return port_; }

 private:
  enum class KeyType {
    kStrict,
    kHost,
    kDomain,
    kLegacy,
  };

  UniqueCookieKey(KeyType key_type,
                  std::optional<CookiePartitionKey> partition_key,
                  std::string name,
                  std::string domain,
                  std::string path,
                  std::optional<CookieSourceScheme> source_scheme,
                  std::optional<int> port);

  // Keys of different "types" (i.e., created by different factory functions)
  // are never considered equivalent.
  KeyType key_type_;
  std::optional<CookiePartitionKey> partition_key_;
  std::string name_;
  std::string domain_;
  std::string path_;
  // Nullopt in kLegacy keys; may be nullopt in kDomain and kHost keys.
  std::optional<CookieSourceScheme> source_scheme_;
  // Nullopt in kLegacy and kDomain keys; may be nullopt in kHost keys.
  std::optional<int> port_;
};

}  // namespace net

#endif  // NET_COOKIES_UNIQUE_COOKIE_KEY_H_
