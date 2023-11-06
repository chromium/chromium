// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_update.h"

#include <utility>

#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {

InterestGroupUpdate::InterestGroupUpdate() = default;
InterestGroupUpdate::InterestGroupUpdate(const InterestGroupUpdate&) = default;
InterestGroupUpdate::InterestGroupUpdate(InterestGroupUpdate&&) = default;
InterestGroupUpdate::~InterestGroupUpdate() = default;

InterestGroupUpdateParameter::InterestGroupUpdateParameter() = default;
InterestGroupUpdateParameter::InterestGroupUpdateParameter(
    blink::InterestGroupKey k,
    GURL u,
    url::Origin o)
    : interest_group_key(std::move(k)),
      update_url(std::move(u)),
      joining_origin(std::move(o)) {}
InterestGroupUpdateParameter::~InterestGroupUpdateParameter() = default;

}  // namespace content
