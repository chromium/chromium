// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/local_set_declaration.h"

#include "base/ranges/algorithm.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

LocalSetDeclaration::LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(
    base::flat_map<SchemefulSite, FirstPartySetEntry> set_entries)
    : entries_(std::move(set_entries)) {
  if (!entries_.empty()) {
    // Must not be a singleton set (i.e. must have more than one entry).
    CHECK_GT(entries_.size(), 1u);

    // All provided entries must have the same primary site. I.e., there must
    // only be one set.
    const SchemefulSite& primary = entries_.begin()->second.primary();
    CHECK(base::ranges::all_of(
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

}  // namespace net
