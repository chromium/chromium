// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/storable_impression.h"

#include "base/check_op.h"

namespace content {

StorableImpression::StorableImpression(
    const std::string& impression_data,
    const url::Origin& impression_origin,
    const url::Origin& conversion_origin,
    const url::Origin& reporting_origin,
    base::Time impression_time,
    base::Time expiry_time,
    const base::Optional<int64_t>& impression_id)
    : impression_data_(impression_data),
      impression_origin_(impression_origin),
      conversion_origin_(conversion_origin),
      reporting_origin_(reporting_origin),
      impression_time_(impression_time),
      expiry_time_(expiry_time),
      impression_id_(impression_id) {
  // 30 days is the max allowed expiry for an impression.
  DCHECK_GE(base::TimeDelta::FromDays(30), expiry_time - impression_time);
  DCHECK(!impression_origin.opaque());
  DCHECK(!reporting_origin.opaque());
  DCHECK(!conversion_origin.opaque());
}

StorableImpression::StorableImpression(const StorableImpression& other) =
    default;

StorableImpression::~StorableImpression() = default;

net::SchemefulSite StorableImpression::ConversionDestination() const {
  return net::SchemefulSite(conversion_origin_);
}

}  // namespace content
