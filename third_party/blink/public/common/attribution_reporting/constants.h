// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_CONSTANTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_CONSTANTS_H_

namespace blink {

constexpr size_t kMaxBytesPerAttributionFilterString = 25;
constexpr size_t kMaxValuesPerAttributionFilter = 50;
constexpr size_t kMaxAttributionFiltersPerSource = 50;

constexpr size_t kMaxBytesPerAttributionAggregationKeyId = 25;
constexpr size_t kMaxAttributionAggregationKeysPerSourceOrTrigger = 50;

constexpr size_t kMaxAttributionAggregatableTriggerDataPerTrigger = 50;

constexpr size_t kMaxAttributionEventTriggerData = 10;

constexpr int kMaxAttributionAggregatableValue = 65536;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_CONSTANTS_H_
