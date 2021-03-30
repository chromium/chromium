// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets.h"

#include <initializer_list>
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

FirstPartySets::FirstPartySets() = default;

FirstPartySets::~FirstPartySets() = default;

void FirstPartySets::SetManuallySpecifiedSet(const std::string& flag_value) {
  manually_specified_set_ = CanonicalizeSet(base::SplitString(
      flag_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  ApplyManuallySpecifiedSet();
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>*
FirstPartySets::ParseAndSet(base::StringPiece raw_sets) {
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

bool FirstPartySets::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite& top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  const auto it = sets_.find(site);
  if (it == sets_.end())
    return false;
  const net::SchemefulSite& site_owner = it->second;
  const auto is_owned_by_site_owner =
      [this, &site_owner](const net::SchemefulSite& context_site) {
        const auto context_owner = sets_.find(context_site);
        return context_owner != sets_.end() &&
               context_owner->second == site_owner;
      };
  return is_owned_by_site_owner(top_frame_site) &&
         base::ranges::all_of(party_context, is_owned_by_site_owner);
}

bool FirstPartySets::IsInNontrivialFirstPartySet(
    const net::SchemefulSite& site) const {
  return base::Contains(sets_, site);
}

base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>
FirstPartySets::Sets() const {
  base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>> sets;

  for (const auto& pair : sets_) {
    const net::SchemefulSite& member = pair.first;
    const net::SchemefulSite& owner = pair.second;
    auto set = sets.find(owner);
    if (set == sets.end()) {
      sets.emplace(owner, std::initializer_list<net::SchemefulSite>{member});
    } else {
      set->second.insert(member);
    }
  }

  return sets;
}

void FirstPartySets::ApplyManuallySpecifiedSet() {
  if (!manually_specified_set_)
    return;

  const net::SchemefulSite& manual_owner = manually_specified_set_->first;
  const base::flat_set<net::SchemefulSite>& manual_members =
      manually_specified_set_->second;

  const auto was_manually_provided =
      [&manual_members, &manual_owner](const net::SchemefulSite& site) {
        return site == manual_owner || manual_members.contains(site);
      };

  // Erase the intersection between the manually-specified set and the
  // CU-supplied set, and any members whose owner was in the intersection.
  sets_.erase(base::ranges::remove_if(sets_,
                                      [&was_manually_provided](const auto& p) {
                                        return was_manually_provided(p.first) ||
                                               was_manually_provided(p.second);
                                      }),
              sets_.end());

  // Now remove singleton sets. We already removed any sites that were part
  // of the intersection, or whose owner was part of the intersection. This
  // leaves sites that *are* owners, which no longer have any (other)
  // members.
  std::set<net::SchemefulSite> owners_with_members;
  for (const auto& it : sets_) {
    if (it.first != it.second)
      owners_with_members.insert(it.second);
  }
  sets_.erase(base::ranges::remove_if(
                  sets_,
                  [&owners_with_members](const auto& p) {
                    return p.first == p.second &&
                           !base::Contains(owners_with_members, p.first);
                  }),
              sets_.end());

  // Next, we must add the manually-added set to the parsed value.
  for (const net::SchemefulSite& member : manual_members) {
    sets_.emplace(member, manual_owner);
  }
  sets_.emplace(manual_owner, manual_owner);
}

}  // namespace network
