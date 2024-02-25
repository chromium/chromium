// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_set_entry_override.h"

#include <utility>

#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

FirstPartySetEntryOverride::FirstPartySetEntryOverride() = default;
FirstPartySetEntryOverride::FirstPartySetEntryOverride(FirstPartySetEntry entry)
    : entry_(std::move(entry)) {}

FirstPartySetEntryOverride::FirstPartySetEntryOverride(
    FirstPartySetEntryOverride&& other) = default;
FirstPartySetEntryOverride& FirstPartySetEntryOverride::operator=(
    FirstPartySetEntryOverride&& other) = default;
FirstPartySetEntryOverride::FirstPartySetEntryOverride(
    const FirstPartySetEntryOverride& other) = default;
FirstPartySetEntryOverride& FirstPartySetEntryOverride::operator=(
    const FirstPartySetEntryOverride& other) = default;

FirstPartySetEntryOverride::~FirstPartySetEntryOverride() = default;

bool FirstPartySetEntryOverride::operator==(
    const FirstPartySetEntryOverride& other) const = default;

std::ostream& operator<<(std::ostream& os,
                         const FirstPartySetEntryOverride& override) {
  os << "{";
  if (override.IsDeletion()) {
    os << "<deleted>";
  } else {
    os << override.GetEntry();
  }
  os << "}";
  return os;
}

}  // namespace net
