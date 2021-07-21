// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/storable_impression.h"

#include "base/check_op.h"
#include "net/base/schemeful_site.h"

namespace content {

StorableImpression::StorableImpression(uint64_t impression_data,
                                       url::Origin impression_origin,
                                       url::Origin conversion_origin,
                                       url::Origin reporting_origin,
                                       base::Time impression_time,
                                       base::Time expiry_time,
                                       SourceType source_type,
                                       int64_t priority,
                                       absl::optional<int64_t> impression_id)
    : impression_data_(impression_data),
      impression_origin_(std::move(impression_origin)),
      conversion_origin_(std::move(conversion_origin)),
      reporting_origin_(std::move(reporting_origin)),
      impression_time_(impression_time),
      expiry_time_(expiry_time),
      source_type_(source_type),
      priority_(priority),
      impression_id_(impression_id) {
  // 30 days is the max allowed expiry for an impression.
  DCHECK_GE(base::TimeDelta::FromDays(30), expiry_time - impression_time);
  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time, impression_time);
  DCHECK(!impression_origin.opaque());
  DCHECK(!reporting_origin.opaque());
  DCHECK(!conversion_origin.opaque());
}

StorableImpression::StorableImpression(const StorableImpression& other) =
    default;

StorableImpression& StorableImpression::operator=(
    const StorableImpression& other) = default;

StorableImpression::StorableImpression(StorableImpression&& other) = default;

StorableImpression& StorableImpression::operator=(StorableImpression&& other) =
    default;

StorableImpression::~StorableImpression() = default;

net::SchemefulSite StorableImpression::ConversionDestination() const {
  return net::SchemefulSite(conversion_origin_);
}

net::SchemefulSite StorableImpression::ImpressionSite() const {
  return net::SchemefulSite(impression_origin_);
}

}  // namespace content
