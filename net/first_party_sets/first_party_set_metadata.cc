// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_set_metadata.h"

#include <tuple>

#include "base/types/optional_util.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

FirstPartySetMetadata::FirstPartySetMetadata() = default;
FirstPartySetMetadata::FirstPartySetMetadata(
    const FirstPartySetEntry* frame_entry,
    const FirstPartySetEntry* top_frame_entry)
    : frame_entry_(base::OptionalFromPtr(frame_entry)),
      top_frame_entry_(base::OptionalFromPtr(top_frame_entry)) {}

FirstPartySetMetadata::FirstPartySetMetadata(FirstPartySetMetadata&&) = default;
FirstPartySetMetadata& FirstPartySetMetadata::operator=(
    FirstPartySetMetadata&&) = default;

FirstPartySetMetadata::~FirstPartySetMetadata() = default;

bool FirstPartySetMetadata::operator==(
    const FirstPartySetMetadata& other) const = default;

bool FirstPartySetMetadata::operator!=(
    const FirstPartySetMetadata& other) const = default;

std::ostream& operator<<(std::ostream& os,
                         const FirstPartySetMetadata& metadata) {
  os << "{" << base::OptionalToPtr(metadata.frame_entry()) << ", "
     << base::OptionalToPtr(metadata.top_frame_entry()) << "}";
  return os;
}

bool FirstPartySetMetadata::AreSitesInSameFirstPartySet() const {
  if (!frame_entry_ || !top_frame_entry_)
    return false;
  return frame_entry_->primary() == top_frame_entry_->primary();
}

}  // namespace net
