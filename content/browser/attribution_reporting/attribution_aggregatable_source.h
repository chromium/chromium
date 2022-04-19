// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This is a wrapper of `proto::AttributionAggregatableSource`.
class CONTENT_EXPORT AttributionAggregatableSource {
 public:
  using Keys = base::flat_map<std::string, absl::uint128>;

  // Returns `absl::nullopt` if `keys` is invalid.
  static absl::optional<AttributionAggregatableSource> FromKeys(Keys keys);

  // Deserializes `str`, if valid. Returns `absl::nullopt` if not.
  static absl::optional<AttributionAggregatableSource> Deserialize(
      const std::string& str);

  AttributionAggregatableSource();
  ~AttributionAggregatableSource();

  AttributionAggregatableSource(const AttributionAggregatableSource&);
  AttributionAggregatableSource(AttributionAggregatableSource&&);

  AttributionAggregatableSource& operator=(
      const AttributionAggregatableSource&);
  AttributionAggregatableSource& operator=(AttributionAggregatableSource&&);

  const Keys& keys() const { return keys_; }

  std::string Serialize() const;

 private:
  explicit AttributionAggregatableSource(Keys keys);

  Keys keys_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_
