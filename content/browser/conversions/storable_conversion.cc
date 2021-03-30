// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/storable_conversion.h"

#include <utility>

#include "base/check.h"

namespace content {

StorableConversion::StorableConversion(
    std::string conversion_data,
    net::SchemefulSite conversion_destination,
    url::Origin reporting_origin)
    : conversion_data_(std::move(conversion_data)),
      conversion_destination_(std::move(conversion_destination)),
      reporting_origin_(std::move(reporting_origin)) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!conversion_destination_.opaque());
}

StorableConversion::StorableConversion(const StorableConversion& other) =
    default;

StorableConversion::~StorableConversion() = default;

}  // namespace content
