// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/preloaded_first_party_sets.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/first_party_sets/first_party_set_parser.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Ensures that the string represents an origin that is non-opaque and HTTPS.
// Returns the registered domain.
base::Optional<std::string> CanonicalizeRegisteredDomain(
    const std::string& origin_string) {
  const url::Origin origin(url::Origin::Create(GURL(origin_string)));
  if (origin.opaque()) {
    LOG(ERROR) << "First-Party Set origin " << origin_string
               << " is not valid; ignoring.";
    return base::nullopt;
  }
  if (origin.scheme() != "https") {
    LOG(ERROR) << "First-Party Set origin " << origin_string
               << " is not HTTPS; ignoring.";
    return base::nullopt;
  }
  const std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (domain_and_registry.empty()) {
    LOG(ERROR) << "First-Party Set origin" << origin_string
               << " does not have a valid registered domain; ignoring.";
    return base::nullopt;
  }

  return domain_and_registry;
}

base::Optional<std::pair<std::string, base::flat_set<std::string>>>
CanonicalizeSet(const std::vector<std::string>& origins) {
  if (origins.empty())
    return base::nullopt;

  const base::Optional<std::string> maybe_owner =
      CanonicalizeRegisteredDomain(origins[0]);
  if (!maybe_owner.has_value()) {
    LOG(ERROR) << "First-Party Set owner is not valid; aborting.";
    return base::nullopt;
  }

  const std::string& owner = *maybe_owner;
  base::flat_set<std::string> members;
  for (auto it = origins.begin() + 1; it != origins.end(); ++it) {
    const base::Optional<std::string> maybe_member =
        CanonicalizeRegisteredDomain(*it);
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

namespace network {

PreloadedFirstPartySets::PreloadedFirstPartySets() = default;

PreloadedFirstPartySets::~PreloadedFirstPartySets() = default;

void PreloadedFirstPartySets::SetManuallySpecifiedSet(
    const std::string& flag_value) {
  manually_specified_set_ = CanonicalizeSet(base::SplitString(
      flag_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  ApplyManuallySpecifiedSet(sets_);
}

base::flat_map<std::string, std::string>* PreloadedFirstPartySets::ParseAndSet(
    base::StringPiece raw_sets) {
  std::unique_ptr<base::flat_map<std::string, std::string>> parsed =
      FirstPartySetParser::ParsePreloadedSets(raw_sets);
  if (parsed) {
    ApplyManuallySpecifiedSet(*parsed);
    sets_.swap(*parsed);
  }
  return &sets_;
}

void PreloadedFirstPartySets::ApplyManuallySpecifiedSet(
    base::flat_map<std::string, std::string>& sets) const {
  if (!manually_specified_set_)
    return;

  const std::string& manual_owner = manually_specified_set_->first;
  const base::flat_set<std::string>& manual_members =
      manually_specified_set_->second;

  sets.erase(base::ranges::remove_if(
                 sets,
                 [&manual_members, &manual_owner](const auto& p) {
                   return p.first == manual_owner || p.second == manual_owner ||
                          manual_members.contains(p.first) ||
                          manual_members.contains(p.second);
                 }),
             sets.end());

  // Next, we must add the manually-added set to the parsed value.
  for (const std::string& member : manual_members) {
    sets.emplace(member, manual_owner);
  }
}

}  // namespace network
