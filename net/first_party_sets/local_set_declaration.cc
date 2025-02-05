// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/local_set_declaration.h"

#include <algorithm>

#include "base/containers/map_util.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/sets_mutation.h"

namespace net {

LocalSetDeclaration::LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(
    base::flat_map<SchemefulSite, FirstPartySetEntry> set_entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : entries_(std::move(set_entries)), aliases_(std::move(aliases)) {
  CHECK(std::ranges::all_of(aliases_, [&](const auto& p) {
    const FirstPartySetEntry* alias_entry = base::FindOrNull(entries_, p.first);
    const FirstPartySetEntry* canonical_entry =
        base::FindOrNull(entries_, p.second);
    // The canonical entry must exist. If the alias entry exists explicitly, it
    // must be the same as the canonical entry.
    return canonical_entry &&
           (!alias_entry || *alias_entry == *canonical_entry);
  }));

  if (!entries_.empty()) {
    // Must not be a singleton set (i.e. must have more than one entry).
    CHECK_GT(entries_.size() + aliases_.size(), 1u);

    // All provided entries must have the same primary site. I.e., there must
    // only be one set.
    const SchemefulSite& primary = entries_.begin()->second.primary();
    CHECK(std::ranges::all_of(
        entries_,
        [&](const std::pair<SchemefulSite, FirstPartySetEntry>& pair) {
          return pair.second.primary() == primary;
        }));
  }
}

LocalSetDeclaration::~LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(const LocalSetDeclaration&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(
    const LocalSetDeclaration&) = default;

LocalSetDeclaration::LocalSetDeclaration(LocalSetDeclaration&&) = default;
LocalSetDeclaration& LocalSetDeclaration::operator=(LocalSetDeclaration&&) =
    default;

SetsMutation LocalSetDeclaration::ComputeMutation() const {
  base::flat_map<SchemefulSite, FirstPartySetEntry> entries = entries_;

  for (const auto& [alias, canonical] : aliases_) {
    entries.emplace(alias, entries.find(canonical)->second);
  }
  // A local set declaration is treated as a "replacement" set.
  return SetsMutation(/*replacement_sets=*/{std::move(entries)},
                      /*addition_sets=*/{}, aliases_);
}

}  // namespace net
