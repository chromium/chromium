// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATION_KEYS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATION_KEYS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace content {

class CONTENT_EXPORT AttributionAggregationKeys {
 public:
  using Keys = base::flat_map<std::string, absl::uint128>;

  // Returns `absl::nullopt` if `keys` is invalid.
  static absl::optional<AttributionAggregationKeys> FromKeys(Keys keys);

  static absl::optional<AttributionAggregationKeys> FromJSON(
      const base::Value*);

  // Deserializes `str`, if valid. Returns `absl::nullopt` if not.
  static absl::optional<AttributionAggregationKeys> Deserialize(
      const std::string& str);

  AttributionAggregationKeys();
  ~AttributionAggregationKeys();

  AttributionAggregationKeys(const AttributionAggregationKeys&);
  AttributionAggregationKeys(AttributionAggregationKeys&&);

  AttributionAggregationKeys& operator=(const AttributionAggregationKeys&);
  AttributionAggregationKeys& operator=(AttributionAggregationKeys&&);

  const Keys& keys() const { return keys_; }

  std::string Serialize() const;

 private:
  explicit AttributionAggregationKeys(Keys keys);

  Keys keys_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATION_KEYS_H_
