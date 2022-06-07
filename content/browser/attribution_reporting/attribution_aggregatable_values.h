// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_VALUES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_VALUES_H_

#include <stdint.h>

#include <string>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT AttributionAggregatableValues {
 public:
  using Values = base::flat_map<std::string, uint32_t>;

  static absl::optional<AttributionAggregatableValues> FromValues(
      Values values);

  static AttributionAggregatableValues CreateForTesting(Values values);

  AttributionAggregatableValues();
  ~AttributionAggregatableValues();

  AttributionAggregatableValues(const AttributionAggregatableValues&);
  AttributionAggregatableValues(AttributionAggregatableValues&&);

  AttributionAggregatableValues& operator=(
      const AttributionAggregatableValues&);
  AttributionAggregatableValues& operator=(AttributionAggregatableValues&&);

  const Values& values() const { return values_; }

 private:
  explicit AttributionAggregatableValues(Values values);

  Values values_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_VALUES_H_
