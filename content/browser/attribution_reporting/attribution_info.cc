// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_info.h"

#include <utility>

namespace content {

AttributionInfo::AttributionInfo(StoredSource source,
                                 base::Time time,
                                 absl::optional<uint64_t> debug_key)
    : source(std::move(source)), time(time), debug_key(debug_key) {}

AttributionInfo::~AttributionInfo() = default;

AttributionInfo::AttributionInfo(const AttributionInfo&) = default;

AttributionInfo::AttributionInfo(AttributionInfo&&) = default;

AttributionInfo& AttributionInfo::operator=(const AttributionInfo&) = default;

AttributionInfo& AttributionInfo::operator=(AttributionInfo&&) = default;

}  // namespace content
