// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

// Ensures that the string represents an origin that is non-opaque and HTTPS.
// Returns the registered domain.
absl::optional<net::SchemefulSite> Canonicalize(base::StringPiece origin_string,
                                                bool emit_errors) {
  const url::Origin origin(url::Origin::Create(GURL(origin_string)));
  if (origin.opaque()) {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin " << origin_string
                 << " is not valid; ignoring.";
    }
    return absl::nullopt;
  }
  if (origin.scheme() != "https") {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin " << origin_string
                 << " is not HTTPS; ignoring.";
    }
    return absl::nullopt;
  }
  absl::optional<net::SchemefulSite> site =
      net::SchemefulSite::CreateIfHasRegisterableDomain(origin);
  if (!site.has_value()) {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin" << origin_string
                 << " does not have a valid registered domain; ignoring.";
    }
    return absl::nullopt;
  }

  return site;
}

const char kFirstPartySetOwnerField[] = "owner";
const char kFirstPartySetMembersField[] = "members";

// Parses a single First-Party Set into a map from member to owner (including an
// entry owner -> owner). Note that this is intended for use *only* on sets that
// were received via the Component Updater, so this does not check assertions
// or versions. It rejects sets which are non-disjoint with
// previously-encountered sets (i.e. sets which have non-empty intersections
// with `elements`), and singleton sets (i.e. sets must have an owner and at
// least one valid member).
//
// Uses `elements` to check disjointness of sets; builds the mapping in `map`;
// and augments `elements` to include the elements of the set that was parsed.
//
// Returns true if parsing and validation were successful, false otherwise.
bool ParseSet(const base::Value& value,
              base::flat_map<net::SchemefulSite, net::SchemefulSite>& map,
              base::flat_set<net::SchemefulSite>& elements) {
  if (!value.is_dict())
    return false;

  // Confirm that the set has an owner, and the owner is a string.
  const std::string* maybe_owner =
      value.FindStringKey(kFirstPartySetOwnerField);
  if (!maybe_owner)
    return false;

  absl::optional<net::SchemefulSite> canonical_owner =
      Canonicalize(std::move(*maybe_owner), false /* emit_errors */);
  if (!canonical_owner.has_value())
    return false;

  // An owner may not be a member of another set.
  if (elements.contains(*canonical_owner))
    return false;

  elements.insert(*canonical_owner);
  map.emplace(*canonical_owner, *canonical_owner);

  // Confirm that the members field is present, and is an array of strings.
  const base::Value* maybe_members_list =
      value.FindListKey(kFirstPartySetMembersField);
  if (!maybe_members_list)
    return false;

  // Add each member to our mapping (assuming the member is a string).
  for (const auto& item : maybe_members_list->GetList()) {
    // Members may not be a member of another set, and may not be an owner of
    // another set.
    if (!item.is_string())
      return false;
    absl::optional<net::SchemefulSite> member =
        Canonicalize(item.GetString(), false /* emit_errors */);
    if (!member.has_value() || elements.contains(*member))
      return false;
    map.emplace(*member, *canonical_owner);
    elements.insert(std::move(*member));
  }
  return !maybe_members_list->GetList().empty();
}

}  // namespace

absl::optional<net::SchemefulSite>
FirstPartySetParser::CanonicalizeRegisteredDomain(
    const base::StringPiece origin_string,
    bool emit_errors) {
  return Canonicalize(origin_string, emit_errors);
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>
FirstPartySetParser::ParseSetsFromComponentUpdater(base::StringPiece raw_sets) {
  absl::optional<base::Value> maybe_value = base::JSONReader::Read(
      raw_sets, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!maybe_value.has_value())
    return {};
  if (!maybe_value->is_list())
    return {};

  base::flat_map<net::SchemefulSite, net::SchemefulSite> map;
  base::flat_set<net::SchemefulSite> elements;
  for (const auto& value : maybe_value->GetList()) {
    if (!ParseSet(value, map, elements))
      return {};
  }

  return map;
}

}  // namespace network
