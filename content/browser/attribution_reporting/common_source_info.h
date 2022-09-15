// Copyright 2022 The Chromium Authors
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
      base::Time source_time,
      AttributionSourceType source_type);

  CommonSourceInfo(uint64_t source_event_id,
                   url::Origin source_origin,
                   url::Origin destination_origin,
                   url::Origin reporting_origin,
                   base::Time source_time,
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

  const url::Origin& source_origin() const { return source_origin_; }

  const url::Origin& destination_origin() const { return destination_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  base::Time source_time() const { return source_time_; }

  base::Time expiry_time() const { return expiry_time_; }

  AttributionSourceType source_type() const { return source_type_; }

  int64_t priority() const { return priority_; }

  const AttributionFilterData& filter_data() const { return filter_data_; }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  const AttributionAggregationKeys& aggregation_keys() const {
    return aggregation_keys_;
  }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

  // Returns the schemeful site of `destination_origin_`.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of `destination_origin_`.
  net::SchemefulSite DestinationSite() const;

  // Returns the schemeful site of |source_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |source_origin_|.
  net::SchemefulSite SourceSite() const;

 private:
  uint64_t source_event_id_;
  url::Origin source_origin_;
  url::Origin destination_origin_;
  url::Origin reporting_origin_;
  base::Time source_time_;
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
