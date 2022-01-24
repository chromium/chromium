// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_source.h"

#include "base/check_op.h"
#include "net/base/schemeful_site.h"

namespace content {

StorableSource::StorableSource(uint64_t source_event_id,
                               url::Origin impression_origin,
                               url::Origin conversion_origin,
                               url::Origin reporting_origin,
                               base::Time impression_time,
                               base::Time expiry_time,
                               SourceType source_type,
                               int64_t priority,
                               AttributionLogic attribution_logic,
                               absl::optional<Id> impression_id)
    : source_event_id_(source_event_id),
      impression_origin_(std::move(impression_origin)),
      conversion_origin_(std::move(conversion_origin)),
      reporting_origin_(std::move(reporting_origin)),
      impression_time_(impression_time),
      expiry_time_(expiry_time),
      source_type_(source_type),
      priority_(priority),
      attribution_logic_(attribution_logic),
      impression_id_(impression_id) {
  // 30 days is the max allowed expiry for an impression.
  DCHECK_GE(base::Days(30), expiry_time - impression_time);
  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time, impression_time);
  DCHECK(!impression_origin.opaque());
  DCHECK(!reporting_origin.opaque());
  DCHECK(!conversion_origin.opaque());
}

StorableSource::StorableSource(const StorableSource& other) = default;

StorableSource& StorableSource::operator=(const StorableSource& other) =
    default;

StorableSource::StorableSource(StorableSource&& other) = default;

StorableSource& StorableSource::operator=(StorableSource&& other) = default;

StorableSource::~StorableSource() = default;

net::SchemefulSite StorableSource::ConversionDestination() const {
  return net::SchemefulSite(conversion_origin_);
}

net::SchemefulSite StorableSource::ImpressionSite() const {
  return net::SchemefulSite(impression_origin_);
}

}  // namespace content
