// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FILTER_DATA_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FILTER_DATA_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace content {

using AttributionFilterValues =
    base::flat_map<std::string, std::vector<std::string>>;

// Set on sources.
// Supports persistence to disk via serializaton to/from proto.
class CONTENT_EXPORT AttributionFilterData {
 public:
  static constexpr char kSourceTypeFilterKey[] = "source_type";

  // Filter data is not allowed to contain a `source_type` filter.
  static absl::optional<AttributionFilterData> Create(AttributionFilterValues);

  static base::expected<AttributionFilterData,
                        attribution_reporting::mojom::SourceRegistrationError>
  FromJSON(base::Value*);

  AttributionFilterData();

  ~AttributionFilterData();

  AttributionFilterData(const AttributionFilterData&);
  AttributionFilterData(AttributionFilterData&&);

  AttributionFilterData& operator=(const AttributionFilterData&);
  AttributionFilterData& operator=(AttributionFilterData&&);

  const AttributionFilterValues& filter_values() const {
    return filter_values_;
  }

 private:
  explicit AttributionFilterData(AttributionFilterValues);

  AttributionFilterValues filter_values_;
};

// Set on triggers.
class CONTENT_EXPORT AttributionFilters {
 public:
  // Filters are allowed to contain a `source_type` filter.
  static absl::optional<AttributionFilters> Create(AttributionFilterValues);

  // Returns filters that match only the given source type.
  static AttributionFilters ForSourceType(AttributionSourceType);

  AttributionFilters();

  ~AttributionFilters();

  AttributionFilters(const AttributionFilters&);
  AttributionFilters(AttributionFilters&&);

  AttributionFilters& operator=(const AttributionFilters&);
  AttributionFilters& operator=(AttributionFilters&&);

  const AttributionFilterValues& filter_values() const {
    return filter_values_;
  }

 private:
  explicit AttributionFilters(AttributionFilterValues);

  AttributionFilterValues filter_values_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FILTER_DATA_H_
