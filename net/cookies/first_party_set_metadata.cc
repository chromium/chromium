// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/first_party_set_metadata.h"

#include <tuple>

#include "base/stl_util.h"
#include "net/cookies/first_party_set_entry.h"

namespace net {

FirstPartySetMetadata::FirstPartySetMetadata() = default;
FirstPartySetMetadata::FirstPartySetMetadata(
    const SamePartyContext& context,
    const FirstPartySetEntry* frame_entry,
    const FirstPartySetEntry* top_frame_entry)
    : context_(context),
      frame_entry_(base::OptionalFromPtr(frame_entry)),
      top_frame_entry_(base::OptionalFromPtr(top_frame_entry)) {}

FirstPartySetMetadata::FirstPartySetMetadata(FirstPartySetMetadata&&) = default;
FirstPartySetMetadata& FirstPartySetMetadata::operator=(
    FirstPartySetMetadata&&) = default;

FirstPartySetMetadata::~FirstPartySetMetadata() = default;

bool FirstPartySetMetadata::operator==(
    const FirstPartySetMetadata& other) const {
  return std::tie(context_, frame_entry_, top_frame_entry_) ==
         std::tie(other.context_, other.frame_entry_, other.top_frame_entry_);
}

std::ostream& operator<<(std::ostream& os,
                         const FirstPartySetMetadata& metadata) {
  os << "{" << metadata.context() << ", "
     << base::OptionalOrNullptr(metadata.frame_entry()) << ", "
     << base::OptionalOrNullptr(metadata.top_frame_entry()) << "}";
  return os;
}

}  // namespace net
