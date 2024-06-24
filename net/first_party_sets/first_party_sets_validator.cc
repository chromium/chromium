// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_validator.h"

#include "base/ranges/algorithm.h"
#include "net/base/schemeful_site.h"

namespace net {

FirstPartySetsValidator::FirstPartySetsValidator() = default;
FirstPartySetsValidator::~FirstPartySetsValidator() = default;

FirstPartySetsValidator::FirstPartySetsValidator(FirstPartySetsValidator&&) =
    default;
FirstPartySetsValidator& FirstPartySetsValidator::operator=(
    FirstPartySetsValidator&&) = default;

void FirstPartySetsValidator::Update(const SchemefulSite& site,
                                     const SchemefulSite& primary) {
  if (site == primary) {
    primary_states_[primary].has_self_entry = true;
  } else {
    primary_states_[primary].has_nonself_entry = true;
  }
}

bool FirstPartySetsValidator::IsValid() const {
  // We should have seen the self-entry and non-self-entry-mentions for every
  // entry's primary site, assuming there are no singletons and no orphans. If
  // we didn't, then there's at least one singleton or orphan.
  return base::ranges::all_of(
      primary_states_,
      [](const std::pair<SchemefulSite, PrimarySiteState>& pair) -> bool {
        return pair.second.has_nonself_entry && pair.second.has_self_entry;
      });
}

bool FirstPartySetsValidator::IsSitePrimaryValid(
    const SchemefulSite& primary) const {
  const auto it = primary_states_.find(primary);
  if (it == primary_states_.end()) {
    return false;
  }
  const PrimarySiteState& state = it->second;
  return state.has_nonself_entry && state.has_self_entry;
}

}  // namespace net
