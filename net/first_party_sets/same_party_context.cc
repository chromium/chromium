// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/same_party_context.h"

#include <ostream>

namespace net {

SamePartyContext::SamePartyContext(Type context_type)
    : context_type_(context_type) {}

bool SamePartyContext::operator==(const SamePartyContext& other) const {
  return context_type_ == other.context_type_;
}

std::ostream& operator<<(std::ostream& os, const SamePartyContext& spc) {
  os << "{" << static_cast<int>(spc.context_type()) << "}";
  return os;
}

// static
SamePartyContext SamePartyContext::MakeInclusive() {
  return SamePartyContext(Type::kSameParty);
}

}  // namespace net
