// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_entry_builder.h"

#include "base/metrics/metrics_hashes.h"

namespace ukm {

UkmEntryBuilder::UkmEntryBuilder(ukm::SourceId source_id,
                                 base::StringPiece event_name)
    : ukm::internal::UkmEntryBuilderBase(source_id,
                                         base::HashMetricName(event_name)) {}

UkmEntryBuilder::~UkmEntryBuilder() {}

void UkmEntryBuilder::SetMetric(base::StringPiece metric_name, int64_t value) {
  SetMetricInternal(base::HashMetricName(metric_name), value);
}

}  // namespace ukm
