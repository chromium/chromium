// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"

namespace blink {

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::AddBaselineRequest(
    const NGBaselineRequest& request) {
  for (const auto& existing : baseline_requests_) {
    if (existing == request)
      return *this;
  }
  baseline_requests_.push_back(request);
  return *this;
}

}  // namespace blink
