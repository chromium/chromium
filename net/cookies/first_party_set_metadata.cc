// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/first_party_set_metadata.h"

#include <tuple>

#include "base/stl_util.h"
#include "net/cookies/cookie_constants.h"

namespace net {

FirstPartySetMetadata::FirstPartySetMetadata() = default;
FirstPartySetMetadata::FirstPartySetMetadata(
    const SamePartyContext& context,
    const SchemefulSite* owner,
    FirstPartySetsContextType first_party_sets_context_type)
    : context_(context),
      owner_(base::OptionalFromPtr(owner)),
      first_party_sets_context_type_(first_party_sets_context_type) {}

FirstPartySetMetadata::FirstPartySetMetadata(FirstPartySetMetadata&&) = default;
FirstPartySetMetadata& FirstPartySetMetadata::operator=(
    FirstPartySetMetadata&&) = default;

FirstPartySetMetadata::~FirstPartySetMetadata() = default;

bool FirstPartySetMetadata::operator==(
    const FirstPartySetMetadata& other) const {
  return std::make_tuple(context(), owner(), first_party_sets_context_type()) ==
         std::make_tuple(other.context(), other.owner(),
                         other.first_party_sets_context_type());
}

std::ostream& operator<<(std::ostream& os, const FirstPartySetMetadata& fpsm) {
  os << "{" << fpsm.context() << ", " << base::OptionalOrNullptr(fpsm.owner())
     << ", " << static_cast<int>(fpsm.first_party_sets_context_type()) << "}";
  return os;
}

}  // namespace net
