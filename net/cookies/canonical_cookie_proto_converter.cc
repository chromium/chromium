// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "canonical_cookie_proto_converter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/types/expected.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "testing/libfuzzer/proto/url_proto_converter.h"
#include "url/gurl.h"

namespace canonical_cookie_proto {

namespace {
net::CookiePriority GetCookiePriority(
    canonical_cookie_proto::CanonicalCookie_Priority priority) {
  switch (priority) {
    case canonical_cookie_proto::CanonicalCookie_Priority_LOW:
      return net::CookiePriority::COOKIE_PRIORITY_LOW;
    case canonical_cookie_proto::CanonicalCookie_Priority_MEDIUM:
      return net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
    case canonical_cookie_proto::CanonicalCookie_Priority_HIGH:
      return net::CookiePriority::COOKIE_PRIORITY_HIGH;
  }
  NOTREACHED();
}

net::CookieSameSite GetCookieSameSite(
    canonical_cookie_proto::CanonicalCookie_CookieSameSite same_site) {
  switch (same_site) {
    case canonical_cookie_proto::CanonicalCookie_CookieSameSite_UNSPECIFIED:
      return net::CookieSameSite::UNSPECIFIED;
    case canonical_cookie_proto::CanonicalCookie_CookieSameSite_NO_RESTRICTION:
      return net::CookieSameSite::NO_RESTRICTION;
    case canonical_cookie_proto::CanonicalCookie_CookieSameSite_LAX_MODE:
      return net::CookieSameSite::LAX_MODE;
    case canonical_cookie_proto::CanonicalCookie_CookieSameSite_STRICT_MODE:
      return net::CookieSameSite::STRICT_MODE;
  }
  NOTREACHED();
}
}  // namespace
std::optional<net::CookiePartitionKey> PartitionKeyFromProto(
    const canonical_cookie_proto::CookiePartitionKey& partition_key_proto) {
  const std::string top_level_site =
      url_proto::Convert(partition_key_proto.schemeful_site());
  const bool has_cross_site_ancestor =
      partition_key_proto.has_cross_site_ancestor();
  const base::expected<net::CookiePartitionKey, std::string> partition_key =
      net::CookiePartitionKey::FromUntrustedInput(top_level_site,
                                                  has_cross_site_ancestor);
  if (partition_key.has_value()) {
    return std::move(partition_key).value();
  }
  return std::nullopt;
}

std::unique_ptr<net::CanonicalCookie> Convert(
    const canonical_cookie_proto::CanonicalCookie& cookie) {
  const GURL url(url_proto::Convert(cookie.url()));

  if (!url.is_valid()) {
    return nullptr;
  }

  const base::Time creation =
      base::Time::FromMillisecondsSinceUnixEpoch(cookie.creation_time());
  const base::Time expiration =
      base::Time::FromMillisecondsSinceUnixEpoch(cookie.expiration_time());
  const base::Time last_access =
      base::Time::FromMillisecondsSinceUnixEpoch(cookie.last_access_time());

  std::optional<net::CookiePartitionKey> partition_key;
  if (cookie.has_partition_key()) {
    partition_key = PartitionKeyFromProto(cookie.partition_key());
  }

  std::unique_ptr<net::CanonicalCookie> sanitized_cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, cookie.name(), cookie.value(), cookie.domain(), cookie.path(),
          creation, expiration, last_access, cookie.secure(),
          cookie.http_only(), GetCookieSameSite(cookie.same_site()),
          GetCookiePriority(cookie.priority()), std::move(partition_key),
          /*status=*/nullptr);
  return sanitized_cookie;
}
}  // namespace canonical_cookie_proto
