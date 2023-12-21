// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/sets_mutation.h"

#include <map>
#include <utility>

#include "base/ranges/algorithm.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

SetsMutation::SetsMutation(
    std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
        replacement_sets,
    std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
        addition_sets)
    : replacements_(std::move(replacement_sets)),
      additions_(std::move(addition_sets)) {
  std::map<SchemefulSite, int> site_counts;

  for (const auto& set : replacements_) {
    for (const auto& [site, unused_entry] : set) {
      site_counts[site]++;
    }
  }
  for (const auto& set : additions_) {
    for (const auto& [site, unused_entry] : set) {
      site_counts[site]++;
    }
  }
  CHECK(base::ranges::all_of(
      site_counts,
      [](const std::pair<SchemefulSite, int> p) { return p.second == 1; }));
}

SetsMutation::SetsMutation() = default;
SetsMutation::SetsMutation(SetsMutation&&) = default;
SetsMutation& SetsMutation::operator=(SetsMutation&&) = default;
SetsMutation::SetsMutation(const SetsMutation&) = default;
SetsMutation& SetsMutation::operator=(const SetsMutation&) = default;
SetsMutation::~SetsMutation() = default;

bool SetsMutation::operator==(const SetsMutation& other) const = default;

std::ostream& operator<<(std::ostream& os, const SetsMutation& mutation) {
  os << "replacements: {";
  for (const auto& set : mutation.replacements()) {
    for (const auto& pair : set) {
      os << pair.first << " -> " << pair.second << ", ";
    }
  }
  os << "}, additions: {";
  for (const auto& set : mutation.additions()) {
    for (const auto& pair : set) {
      os << pair.first << " -> " << pair.second << ", ";
    }
  }
  os << "}";
  return os;
}

}  // namespace net
