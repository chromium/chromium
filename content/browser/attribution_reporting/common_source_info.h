// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

constexpr base::TimeDelta kDefaultAttributionSourceExpiry = base::Days(30);

// Contains common attributes of `StorableSource` and `StoredSource`.
class CONTENT_EXPORT CommonSourceInfo {
 public:
  static base::Time GetExpiryTime(
      absl::optional<base::TimeDelta> declared_expiry,
      base::Time impression_time,
      AttributionSourceType source_type);

  CommonSourceInfo(uint64_t source_event_id,
                   url::Origin impression_origin,
                   url::Origin conversion_origin,
                   url::Origin reporting_origin,
                   base::Time impression_time,
                   base::Time expiry_time,
                   AttributionSourceType source_type,
                   int64_t priority,
                   AttributionFilterData filter_data,
                   absl::optional<uint64_t> debug_key,
                   AttributionAggregationKeys aggregation_keys);

  ~CommonSourceInfo();

  CommonSourceInfo(const CommonSourceInfo&);
  CommonSourceInfo(CommonSourceInfo&&);

  CommonSourceInfo& operator=(const CommonSourceInfo&);
  CommonSourceInfo& operator=(CommonSourceInfo&&);

  uint64_t source_event_id() const { return source_event_id_; }

  const url::Origin& impression_origin() const { return impression_origin_; }

  const url::Origin& conversion_origin() const { return conversion_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  base::Time impression_time() const { return impression_time_; }

  base::Time expiry_time() const { return expiry_time_; }

  AttributionSourceType source_type() const { return source_type_; }

  int64_t priority() const { return priority_; }

  const AttributionFilterData& filter_data() const { return filter_data_; }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  const AttributionAggregationKeys& aggregation_keys() const {
    return aggregation_keys_;
  }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

  // Returns the schemeful site of |conversion_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |conversion_origin_|.
  net::SchemefulSite ConversionDestination() const;

  // Returns the schemeful site of |impression_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |impression_origin_|.
  net::SchemefulSite ImpressionSite() const;

 private:
  uint64_t source_event_id_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  base::Time impression_time_;
  base::Time expiry_time_;
  AttributionSourceType source_type_;
  int64_t priority_;
  AttributionFilterData filter_data_;
  absl::optional<uint64_t> debug_key_;
  AttributionAggregationKeys aggregation_keys_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
