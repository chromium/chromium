// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/origin_trial_status_change_details.h"

namespace content {

OriginTrialStatusChangeDetails::OriginTrialStatusChangeDetails() = default;
OriginTrialStatusChangeDetails::~OriginTrialStatusChangeDetails() = default;
OriginTrialStatusChangeDetails::OriginTrialStatusChangeDetails(
    const url::Origin& origin,
    const std::string& partition_site,
    bool match_subdomains,
    bool enabled,
    std::optional<ukm::SourceId> source_id)
    : origin(origin),
      partition_site(partition_site),
      match_subdomains(match_subdomains),
      enabled(enabled),
      source_id(source_id) {}

OriginTrialStatusChangeDetails::OriginTrialStatusChangeDetails(
    const OriginTrialStatusChangeDetails&) = default;
OriginTrialStatusChangeDetails& OriginTrialStatusChangeDetails::operator=(
    const OriginTrialStatusChangeDetails&) = default;
bool OriginTrialStatusChangeDetails::operator==(
    const OriginTrialStatusChangeDetails&) const = default;

}  // namespace content
