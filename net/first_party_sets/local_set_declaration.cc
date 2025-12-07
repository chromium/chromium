// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/local_set_declaration.h"

#include <algorithm>
#include <optional>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/sets_mutation.h"

namespace net {

namespace {
bool CheckPreconditions(
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& entries,
    const base::flat_map<SchemefulSite, SchemefulSite>& aliases,
    bool emit_errors) {
  auto emit = [&](std::string_view message) {
    if (emit_errors) {
      LOG(ERROR) << message;
    }
  };

  if (!std::ranges::all_of(aliases, [&](const auto& p) {
        const auto& [alias_site, canonical_site] = p;
        // The canonical entry must exist.
        if (!base::Contains(entries, canonical_site)) {
          emit(
              "Invalid local Related Website Set: alias names a site that has "
              "no entry in the set.");
          return false;
        }
        // The alias entry must not exist explicitly.
        if (base::Contains(entries, alias_site)) {
          emit(
              "Invalid local Related Website Set: alias site should not be "
              "listed in `entries`.");
          return false;
        }
        return true;
      })) {
    return false;
  }

  if (!entries.empty()) {
    // Must not be a singleton set.
    if (entries.size() + aliases.size() <= 1) {
      emit(
          "Invalid local Related Website Set: set must include more than one "
          "site.");
      return false;
    }

    // All provided entries must have the same primary site. I.e., there must
    // only be one set.
    const SchemefulSite& primary = entries.begin()->second.primary();
    if (!std::ranges::all_of(
            entries,
            [&](const std::pair<SchemefulSite, FirstPartySetEntry>& pair) {
              return pair.second.primary() == primary;
            })) {
      // More than one set was provided. That is (currently) unsupported.
      emit(
          "Invalid local Related Website Set: entries must all list the same "
          "primary site.");
      return false;
    }
  }
  return true;
}
}  // namespace

// static
std::optional<LocalSetDeclaration> LocalSetDeclaration::Create(
    base::flat_map<SchemefulSite, FirstPartySetEntry> set_entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases,
    bool emit_errors) {
  if (!CheckPreconditions(set_entries, aliases, emit_errors)) {
    return std::nullopt;
  }
  return LocalSetDeclaration(std::move(set_entries), std::move(aliases));
}

LocalSetDeclaration::LocalSetDeclaration() = default;

LocalSetDeclaration::LocalSetDeclaration(
    base::flat_map<SchemefulSite, FirstPartySetEntry> set_entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : entries_(std::move(set_entries)), aliases_(std::move(aliases)) {}

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
    // Note: it's safe to dereference the pointer below due to the checks in
    // `CheckPreconditions`.
    entries.emplace(alias, *base::FindOrNull(entries, canonical));
  }
  // A local set declaration is treated as a "replacement" set.
  return SetsMutation(/*replacement_sets=*/{std::move(entries)},
                      /*addition_sets=*/{}, aliases_);
}

}  // namespace net
