// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_REF_UNIQUE_COOKIE_KEY_H_
#define NET_COOKIES_REF_UNIQUE_COOKIE_KEY_H_

#include <compare>
#include <optional>
#include <string_view>

#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

class CookieBase;

// RefUniqueCookieKey is similar to a UniqueCookieKey, but it does not own the
// underlying data. It MUST NOT outlive the CookieBase used to create it,
// because it contains references to data in the CookieBase.
class NET_EXPORT RefUniqueCookieKey {
 public:
  // Conditionally populates the source scheme and source port depending on the
  // state of their associated feature.
  static RefUniqueCookieKey Host(
      base::PassKey<CookieBase>,
      base::optional_ref<const CookiePartitionKey> partition_key,
      std::string_view name,
      std::string_view domain,
      std::string_view path,
      std::optional<CookieSourceScheme> source_scheme,
      std::optional<int> source_port);

  // Same as Host but for use with Domain cookies, which do not
  // consider the source_port.
  static RefUniqueCookieKey Domain(
      base::PassKey<CookieBase>,
      base::optional_ref<const CookiePartitionKey> partition_key,
      std::string_view name,
      std::string_view domain,
      std::string_view path,
      std::optional<CookieSourceScheme> source_scheme);

  RefUniqueCookieKey(RefUniqueCookieKey&&);
  RefUniqueCookieKey(const RefUniqueCookieKey&) = delete;
  RefUniqueCookieKey& operator=(RefUniqueCookieKey&&) = delete;
  RefUniqueCookieKey& operator=(const RefUniqueCookieKey&) = delete;

  ~RefUniqueCookieKey();

  friend bool operator==(const RefUniqueCookieKey&,
                         const RefUniqueCookieKey&) = default;
  friend auto operator<=>(const RefUniqueCookieKey&,
                          const RefUniqueCookieKey&) = default;

 private:
  enum class KeyType {
    kHost,
    kDomain,
  };

  RefUniqueCookieKey(KeyType key_type,
                     base::optional_ref<const CookiePartitionKey> partition_key,
                     std::string_view name,
                     std::string_view domain,
                     std::string_view path,
                     std::optional<CookieSourceScheme> source_scheme,
                     std::optional<int> port);

  // Keys of different "types" (i.e., created by different factory functions)
  // are never considered equivalent.
  KeyType key_type_;
  const base::optional_ref<const CookiePartitionKey> partition_key_;
  const std::string_view name_;
  const std::string_view domain_;
  const std::string_view path_;
  // Nullopt in kLegacy keys; may be nullopt in kDomain and kHost keys.
  const std::optional<CookieSourceScheme> source_scheme_;
  // Nullopt in kLegacy and kDomain keys; may be nullopt in kHost keys.
  const std::optional<int> port_;
};

}  // namespace net

#endif  // NET_COOKIES_REF_UNIQUE_COOKIE_KEY_H_
