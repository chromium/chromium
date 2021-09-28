// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include "base/feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"

namespace net {

CookiePartitionKey::CookiePartitionKey() = default;

CookiePartitionKey::CookiePartitionKey(const SchemefulSite& site)
    : site_(site) {}

CookiePartitionKey::CookiePartitionKey(const GURL& url)
    : site_(SchemefulSite(url)) {}

CookiePartitionKey::CookiePartitionKey(const CookiePartitionKey& other) =
    default;

CookiePartitionKey::CookiePartitionKey(CookiePartitionKey&& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(
    const CookiePartitionKey& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(CookiePartitionKey&& other) =
    default;

CookiePartitionKey::~CookiePartitionKey() = default;

bool CookiePartitionKey::operator==(const CookiePartitionKey& other) const {
  return site_ == other.site_;
}

bool CookiePartitionKey::operator!=(const CookiePartitionKey& other) const {
  return site_ != other.site_;
}

bool CookiePartitionKey::operator<(const CookiePartitionKey& other) const {
  return site_ < other.site_;
}

// static
bool CookiePartitionKey::Serialize(const absl::optional<CookiePartitionKey>& in,
                                   std::string& out) {
  if (!in) {
    out = kEmptyCookiePartitionKey;
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return false;
  if (in->site_.GetURL().SchemeIsFile()) {
    out = in->site_.SerializeFileSiteWithHost();
    return true;
  }
  if (in->site_.opaque())
    return false;
  out = in->site_.Serialize();
  return true;
}

// static
bool CookiePartitionKey::Deserialize(const std::string& in,
                                     absl::optional<CookiePartitionKey>& out) {
  if (in == kEmptyCookiePartitionKey) {
    out = absl::nullopt;
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return false;
  auto schemeful_site = SchemefulSite::Deserialize(in);
  // SchemfulSite is opaque if the input is invalid.
  if (schemeful_site.opaque())
    return false;
  out = absl::make_optional(CookiePartitionKey(schemeful_site));
  return true;
}

absl::optional<CookiePartitionKey> CookiePartitionKey::FromNetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies))
    return absl::nullopt;
  // TODO(crbug.com/1225444): Check if the top frame site is in a First-Party
  // Set or if it is an extension URL.
  absl::optional<net::SchemefulSite> top_frame_site =
      network_isolation_key.GetTopFrameSite();
  if (!top_frame_site)
    return absl::nullopt;
  return absl::make_optional(net::CookiePartitionKey(top_frame_site.value()));
}

}  // namespace net
