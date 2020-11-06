// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/preloaded_first_party_sets.h"

#include <memory>

#include "base/logging.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "net/base/schemeful_site.h"
#include "services/network/first_party_sets/first_party_set_parser.h"

namespace network {

namespace {

base::Optional<
    std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>>
CanonicalizeSet(const std::vector<std::string>& origins) {
  if (origins.empty())
    return base::nullopt;

  const base::Optional<net::SchemefulSite> maybe_owner =
      FirstPartySetParser::CanonicalizeRegisteredDomain(origins[0],
                                                        true /* emit_errors */);
  if (!maybe_owner.has_value()) {
    LOG(ERROR) << "First-Party Set owner is not valid; aborting.";
    return base::nullopt;
  }

  const net::SchemefulSite& owner = *maybe_owner;
  base::flat_set<net::SchemefulSite> members;
  for (auto it = origins.begin() + 1; it != origins.end(); ++it) {
    const base::Optional<net::SchemefulSite> maybe_member =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            *it, true /* emit_errors */);
    if (maybe_member.has_value() && maybe_member != owner)
      members.emplace(std::move(*maybe_member));
  }

  if (members.empty()) {
    LOG(ERROR) << "No valid First-Party Set members were specified; aborting.";
    return base::nullopt;
  }

  return base::make_optional(
      std::make_pair(std::move(owner), std::move(members)));
}

}  // namespace

PreloadedFirstPartySets::PreloadedFirstPartySets() = default;

PreloadedFirstPartySets::~PreloadedFirstPartySets() = default;

void PreloadedFirstPartySets::SetManuallySpecifiedSet(
    const std::string& flag_value) {
  manually_specified_set_ = CanonicalizeSet(base::SplitString(
      flag_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  ApplyManuallySpecifiedSet();
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>*
PreloadedFirstPartySets::ParseAndSet(base::StringPiece raw_sets) {
  std::unique_ptr<base::flat_map<net::SchemefulSite, net::SchemefulSite>>
      parsed = FirstPartySetParser::ParsePreloadedSets(raw_sets);
  if (parsed) {
    sets_.swap(*parsed);
  } else {
    // On any error, we clear the sets, to avoid using the old data and to make
    // the failure as obvious as possible.
    sets_.clear();
  }
  ApplyManuallySpecifiedSet();
  return &sets_;
}

void PreloadedFirstPartySets::ApplyManuallySpecifiedSet() {
  if (!manually_specified_set_)
    return;

  const net::SchemefulSite& manual_owner = manually_specified_set_->first;
  const base::flat_set<net::SchemefulSite>& manual_members =
      manually_specified_set_->second;

  sets_.erase(
      base::ranges::remove_if(sets_,
                              [&manual_members, &manual_owner](const auto& p) {
                                return p.first == manual_owner ||
                                       p.second == manual_owner ||
                                       manual_members.contains(p.first) ||
                                       manual_members.contains(p.second);
                              }),
      sets_.end());

  // Next, we must add the manually-added set to the parsed value.
  for (const net::SchemefulSite& member : manual_members) {
    sets_.emplace(member, manual_owner);
  }
}

}  // namespace network
