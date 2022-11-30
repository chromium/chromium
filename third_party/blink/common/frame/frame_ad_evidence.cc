// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "third_party/blink/public/common/frame/frame_ad_evidence.h"

namespace blink {

mojom::FilterListResult MoreRestrictiveFilterListEvidence(
    mojom::FilterListResult a,
    mojom::FilterListResult b) {
  return std::max(a, b);
}

FrameAdEvidence::FrameAdEvidence(bool parent_is_ad)
    : parent_is_ad_(parent_is_ad) {}

FrameAdEvidence::FrameAdEvidence(const FrameAdEvidence&) = default;

FrameAdEvidence::~FrameAdEvidence() = default;

bool FrameAdEvidence::IndicatesAdFrame() const {
  DCHECK(is_complete_);

  // We tag a frame as an ad if its parent is one, it was created by ad script
  // or the frame has ever navigated to an URL matching a blocking rule.
  return parent_is_ad_ ||
         created_by_ad_script_ ==
             mojom::FrameCreationStackEvidence::kCreatedByAdScript ||
         most_restrictive_filter_list_result_ ==
             mojom::FilterListResult::kMatchedBlockingRule;
}

void FrameAdEvidence::UpdateFilterListResult(mojom::FilterListResult value) {
  latest_filter_list_result_ = value;
  most_restrictive_filter_list_result_ = MoreRestrictiveFilterListEvidence(
      most_restrictive_filter_list_result_, value);
}

}  // namespace blink
