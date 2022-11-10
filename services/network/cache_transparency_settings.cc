// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cache_transparency_settings.h"

#include <iterator>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "services/network/public/cpp/features.h"

namespace network {

CacheTransparencySettings::CacheTransparencySettings()
    : cache_transparency_enabled_(
          base::FeatureList::IsEnabled(features::kCacheTransparency) &&
          base::FeatureList::IsEnabled(features::kPervasivePayloadsList)),
      pervasive_payloads_enabled_(
          base::FeatureList::IsEnabled(features::kPervasivePayloadsList)),
      map_(pervasive_payloads_enabled_ ? CreateMap() : PervasivePayloadsMap()) {
}

CacheTransparencySettings::~CacheTransparencySettings() = default;

absl::optional<int> CacheTransparencySettings::GetIndexForURL(
    const GURL& url) const {
  if (!pervasive_payloads_enabled_ || !url.is_valid())
    return absl::nullopt;

  auto it = map_.find(url.spec());
  if (it == map_.end()) {
    return absl::nullopt;
  }
  return std::distance(map_.begin(), it);
}

absl::optional<std::string> CacheTransparencySettings::GetChecksumForURL(
    const GURL& url) const {
  if (!cache_transparency_enabled_ || !url.is_valid())
    return absl::nullopt;

  auto it = map_.find(url.spec());
  if (it == map_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

// static
CacheTransparencySettings::PervasivePayloadsMap
CacheTransparencySettings::CreateMap() {
  const std::string comma_separated =
      features::kCacheTransparencyPervasivePayloads.Get();
  auto split = base::SplitStringPiece(
      comma_separated, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.empty()) {
    // The code below safely produces an empty map in this case.
    DLOG(WARNING) << "Pervasive payload list is empty.";
  } else {
    // The number of items cannot be large, so this O(N) algorithm is
    // acceptable.
    split.erase(split.begin());
  }
  if (split.size() % 2 == 1) {
    DLOG(WARNING)
        << "Pervasive payload list contains an odd number of elements."
        << comma_separated;
  }
  using Container = PervasivePayloadsMap::container_type;
  Container pairs;
  pairs.reserve(split.size() / 2);
  // `split` has to fit in memory, therefore split.size() cannot be the
  // largest possible value, therefore adding 1 to i will not overflow.
  for (size_t i = 0; i + 1 < split.size(); i += 2) {
    pairs.emplace_back(split[i], split[i + 1]);
  }
  return PervasivePayloadsMap(std::move(pairs));
}

}  // namespace network
