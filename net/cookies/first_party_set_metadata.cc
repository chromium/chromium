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
    const SchemefulSite* frame_owner,
    const SchemefulSite* top_frame_owner,
    FirstPartySetsContextType first_party_sets_context_type)
    : context_(context),
      frame_owner_(base::OptionalFromPtr(frame_owner)),
      top_frame_owner_(base::OptionalFromPtr(top_frame_owner)),
      first_party_sets_context_type_(first_party_sets_context_type) {}

FirstPartySetMetadata::FirstPartySetMetadata(FirstPartySetMetadata&&) = default;
FirstPartySetMetadata& FirstPartySetMetadata::operator=(
    FirstPartySetMetadata&&) = default;

FirstPartySetMetadata::~FirstPartySetMetadata() = default;

bool FirstPartySetMetadata::operator==(
    const FirstPartySetMetadata& other) const {
  return std::tie(context_, frame_owner_, top_frame_owner_,
                  first_party_sets_context_type_) ==
         std::tie(other.context_, other.frame_owner_, other.top_frame_owner_,
                  other.first_party_sets_context_type_);
}

std::ostream& operator<<(std::ostream& os,
                         const FirstPartySetMetadata& metadata) {
  os << "{" << metadata.context() << ", "
     << base::OptionalOrNullptr(metadata.frame_owner()) << ", "
     << base::OptionalOrNullptr(metadata.top_frame_owner()) << ", "
     << static_cast<int>(metadata.first_party_sets_context_type()) << "}";
  return os;
}

}  // namespace net
