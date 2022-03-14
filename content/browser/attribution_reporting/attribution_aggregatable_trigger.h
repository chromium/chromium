// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_key.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"

namespace content {

class CONTENT_EXPORT AttributionAggregatableTriggerData {
 public:
  static absl::optional<AttributionAggregatableTriggerData> FromMojo(
      blink::mojom::AttributionAggregatableTriggerDataPtr mojo);

  AttributionAggregatableTriggerData();
  ~AttributionAggregatableTriggerData();

  AttributionAggregatableTriggerData(const AttributionAggregatableTriggerData&);
  AttributionAggregatableTriggerData(AttributionAggregatableTriggerData&&);

  AttributionAggregatableTriggerData& operator=(
      const AttributionAggregatableTriggerData&);
  AttributionAggregatableTriggerData& operator=(
      AttributionAggregatableTriggerData&&);

  const AttributionAggregatableKey& key() const { return key_; }

  const base::flat_set<std::string>& source_keys() const {
    return source_keys_;
  }

  const AttributionFilterData& filters() const { return filters_; }

  const AttributionFilterData& not_filters() const { return not_filters_; }

 private:
  AttributionAggregatableTriggerData(AttributionAggregatableKey key,
                                     base::flat_set<std::string> source_keys,
                                     AttributionFilterData filters,
                                     AttributionFilterData not_filters);

  AttributionAggregatableKey key_;
  base::flat_set<std::string> source_keys_;
  AttributionFilterData filters_;
  AttributionFilterData not_filters_;
};

class CONTENT_EXPORT AttributionAggregatableTrigger {
 public:
  using Values = base::flat_map<std::string, uint32_t>;

  static absl::optional<AttributionAggregatableTrigger> FromMojo(
      blink::mojom::AttributionAggregatableTriggerPtr mojo);

  AttributionAggregatableTrigger();
  ~AttributionAggregatableTrigger();

  AttributionAggregatableTrigger(const AttributionAggregatableTrigger&);
  AttributionAggregatableTrigger(AttributionAggregatableTrigger&&);

  AttributionAggregatableTrigger& operator=(
      const AttributionAggregatableTrigger&);
  AttributionAggregatableTrigger& operator=(AttributionAggregatableTrigger&&);

  const std::vector<AttributionAggregatableTriggerData>& trigger_data() const {
    return trigger_data_;
  }

  const Values& values() const { return values_; }

 private:
  explicit AttributionAggregatableTrigger(
      std::vector<AttributionAggregatableTriggerData> trigger_data,
      Values values);

  std::vector<AttributionAggregatableTriggerData> trigger_data_;
  Values values_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_TRIGGER_H_
