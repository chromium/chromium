// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_info.h"

#include <utility>

#include "components/attribution_reporting/suitable_origin.h"

namespace content {

// TODO(apaseltiner): DCHECK that `context_origin` is valid for `source` and
// likewise non-DCHECK that this is true when reading reports out of storage.
AttributionInfo::AttributionInfo(
    StoredSource source,
    base::Time time,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::SuitableOrigin context_origin)
    : source(std::move(source)),
      time(time),
      debug_key(debug_key),
      context_origin(std::move(context_origin)) {}

AttributionInfo::~AttributionInfo() = default;

AttributionInfo::AttributionInfo(const AttributionInfo&) = default;

AttributionInfo::AttributionInfo(AttributionInfo&&) = default;

AttributionInfo& AttributionInfo::operator=(const AttributionInfo&) = default;

AttributionInfo& AttributionInfo::operator=(AttributionInfo&&) = default;

}  // namespace content
