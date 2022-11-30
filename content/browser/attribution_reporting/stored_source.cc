// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/stored_source.h"

#include <stdint.h>

#include <utility>

#include "base/check_op.h"

namespace content {

StoredSource::StoredSource(CommonSourceInfo common_info,
                           AttributionLogic attribution_logic,
                           ActiveState active_state,
                           Id source_id,
                           int64_t aggregatable_budget_consumed)
    : common_info_(std::move(common_info)),
      attribution_logic_(attribution_logic),
      active_state_(active_state),
      source_id_(source_id),
      aggregatable_budget_consumed_(aggregatable_budget_consumed) {
  DCHECK_GE(aggregatable_budget_consumed_, 0);
}

StoredSource::~StoredSource() = default;

StoredSource::StoredSource(const StoredSource&) = default;

StoredSource::StoredSource(StoredSource&&) = default;

StoredSource& StoredSource::operator=(const StoredSource&) = default;

StoredSource& StoredSource::operator=(StoredSource&&) = default;

}  // namespace content
