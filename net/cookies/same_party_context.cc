// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/same_party_context.h"

#include <tuple>

#include "net/cookies/cookie_constants.h"

namespace net {

SamePartyContext::SamePartyContext(Type type)
    : SamePartyContext(type, type, type) {}

SamePartyContext::SamePartyContext(Type context_type,
                                   Type ancestors_for_metrics,
                                   Type top_resource_for_metrics)
    : context_type_(context_type),
      ancestors_for_metrics_only_(ancestors_for_metrics),
      top_resource_for_metrics_only_(top_resource_for_metrics) {}

bool SamePartyContext::operator==(const SamePartyContext& other) const {
  return std::make_tuple(context_type(), ancestors_for_metrics_only(),
                         top_resource_for_metrics_only()) ==
         std::make_tuple(other.context_type(),
                         other.ancestors_for_metrics_only(),
                         other.top_resource_for_metrics_only());
}

std::ostream& operator<<(std::ostream& os, const SamePartyContext& spc) {
  os << "{" << static_cast<int>(spc.context_type()) << ", "
     << static_cast<int>(spc.ancestors_for_metrics_only()) << ", "
     << static_cast<int>(spc.top_resource_for_metrics_only()) << "}";
  return os;
}

// static
SamePartyContext SamePartyContext::MakeInclusive() {
  return SamePartyContext(Type::kSameParty);
}
}  // namespace net
