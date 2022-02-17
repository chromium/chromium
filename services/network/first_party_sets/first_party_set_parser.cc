// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
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
  for (const auto& item : maybe_members_list->GetListDeprecated()) {
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
  return !maybe_members_list->GetListDeprecated().empty();
}

}  // namespace

base::flat_map<net::SchemefulSite, net::SchemefulSite>
FirstPartySetParser::DeserializeFirstPartySets(base::StringPiece value) {
  if (value.empty())
    return {};

  std::unique_ptr<base::Value> value_deserialized =
      JSONStringValueDeserializer(value).Deserialize(
          nullptr /* error_code */, nullptr /* error_message */);
  if (!value_deserialized || !value_deserialized->is_dict())
    return {};

  base::flat_map<net::SchemefulSite, net::SchemefulSite> map;
  base::flat_set<net::SchemefulSite> owner_set;
  base::flat_set<net::SchemefulSite> member_set;
  for (const auto item : value_deserialized->DictItems()) {
    if (!item.second.is_string())
      return {};
    const absl::optional<net::SchemefulSite> maybe_member =
        Canonicalize(item.first, true /* emit_errors */);
    const absl::optional<net::SchemefulSite> maybe_owner =
        Canonicalize(item.second.GetString(), true /* emit_errors */);
    if (!maybe_member.has_value() || !maybe_owner.has_value())
      return {};

    // Skip the owner entry here and add it later explicitly to prevent the
    // singleton sets.
    if (*maybe_member == *maybe_owner) {
      continue;
    }
    if (!owner_set.contains(maybe_owner)) {
      map.emplace(*maybe_owner, *maybe_owner);
    }
    // Check disjointness. Note that we are relying on the JSON Parser to
    // eliminate the possibility of a site being used as a key more than once,
    // so we don't have to check for that explicitly.
    if (owner_set.contains(*maybe_member) ||
        member_set.contains(*maybe_owner)) {
      return {};
    }
    owner_set.insert(*maybe_owner);
    member_set.insert(*maybe_member);
    map.emplace(std::move(*maybe_member), std::move(*maybe_owner));
  }
  return map;
}

std::string FirstPartySetParser::SerializeFirstPartySets(
    const base::flat_map<net::SchemefulSite, net::SchemefulSite>& sets) {
  base::DictionaryValue dict;
  for (const auto& it : sets) {
    std::string maybe_member = it.first.Serialize();
    std::string owner = it.second.Serialize();
    if (maybe_member != owner) {
      dict.SetKey(std::move(maybe_member), base::Value(std::move(owner)));
    }
  }
  std::string dict_serialized;
  JSONStringValueSerializer(&dict_serialized).Serialize(dict);

  return dict_serialized;
}

absl::optional<net::SchemefulSite>
FirstPartySetParser::CanonicalizeRegisteredDomain(
    const base::StringPiece origin_string,
    bool emit_errors) {
  return Canonicalize(origin_string, emit_errors);
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>
FirstPartySetParser::ParseSetsFromStream(std::istream& input) {
  base::flat_map<net::SchemefulSite, net::SchemefulSite> map;
  base::flat_set<net::SchemefulSite> elements;
  for (std::string line; std::getline(input, line);) {
    base::StringPiece trimmed = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (trimmed.empty())
      continue;
    absl::optional<base::Value> maybe_value = base::JSONReader::Read(
        trimmed, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (!maybe_value.has_value())
      return {};
    if (!ParseSet(*maybe_value, map, elements))
      return {};
  }

  return map;
}

}  // namespace network
