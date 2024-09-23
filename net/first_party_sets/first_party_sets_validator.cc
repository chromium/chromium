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
  PrimarySiteState& primary_state = primary_states_[primary];
  if (site == primary) {
    primary_state.has_self_entry = true;
  } else {
    primary_state.has_nonself_entry = true;
  }

  const auto [it, inserted] = site_metadatas_.emplace(site, SiteState{primary});
  if (!inserted) {
    // `site` appears in more than one set (or is listed in the same set more
    // than once).
    primary_state.is_disjoint = false;
    primary_states_[it->second.first_seen_primary].is_disjoint = false;
  }
}

bool FirstPartySetsValidator::IsValid() const {
  return base::ranges::all_of(primary_states_, [](const auto& pair) -> bool {
    return pair.second.IsValid();
  });
}

bool FirstPartySetsValidator::IsSitePrimaryValid(
    const SchemefulSite& primary) const {
  const auto it = primary_states_.find(primary);
  return it != primary_states_.end() && it->second.IsValid();
}

bool FirstPartySetsValidator::PrimarySiteState::IsValid() const {
  // A set is valid iff its primary site has a self-entry, has at least one
  // non-self entry, and the set is disjoint from all other sets.
  return has_nonself_entry && has_self_entry && is_disjoint;
}

}  // namespace net
