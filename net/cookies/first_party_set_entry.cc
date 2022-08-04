// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/first_party_set_entry.h"

#include <tuple>

#include "net/base/schemeful_site.h"

namespace net {

FirstPartySetEntry::FirstPartySetEntry() = default;

FirstPartySetEntry::FirstPartySetEntry(SchemefulSite primary)
    : primary_(primary) {}

FirstPartySetEntry::FirstPartySetEntry(const FirstPartySetEntry&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(const FirstPartySetEntry&) =
    default;
FirstPartySetEntry::FirstPartySetEntry(FirstPartySetEntry&&) = default;
FirstPartySetEntry& FirstPartySetEntry::operator=(FirstPartySetEntry&&) =
    default;

FirstPartySetEntry::~FirstPartySetEntry() = default;

bool FirstPartySetEntry::operator==(const FirstPartySetEntry& other) const {
  return primary_ == other.primary_;
}

bool FirstPartySetEntry::operator!=(const FirstPartySetEntry& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const FirstPartySetEntry& entry) {
  os << "{" << entry.primary() << "}";
  return os;
}

}  // namespace net
