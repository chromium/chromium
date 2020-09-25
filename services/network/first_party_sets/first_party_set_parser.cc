// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include <iterator>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"

namespace network {

namespace {

const char kFirstPartySetOwnerField[] = "owner";
const char kFirstPartySetMembersField[] = "members";

// Parses a single First-Party Set into a map from member to owner (not
// including the owner). Note that this is intended for use *only* on sets that
// were preloaded via the component updater, so this does not check assertions
// or versions. It does not handle non-disjoint sets (i.e. sets which have
// non-empty intersections of owners and/or members)..
void ParsePreloadedSet(
    const base::Value& value,
    std::vector<std::pair<std::string, std::string>>& map_storage,
    base::flat_set<std::string>& owners,
    base::flat_set<std::string>& members) {
  if (!value.is_dict())
    return;

  // Confirm that the set has an owner, and the owner is a string.
  const std::string* maybe_owner =
      value.FindStringKey(kFirstPartySetOwnerField);
  if (!maybe_owner)
    return;

  // An owner may only be listed once, and may not be a member of another set.
  if (members.contains(*maybe_owner) || owners.contains(*maybe_owner))
    return;

  owners.insert(*maybe_owner);

  // Confirm that the members field is present, and is an array of strings.
  const base::Value* maybe_members_list =
      value.FindListKey(kFirstPartySetMembersField);
  if (!maybe_members_list)
    return;

  // Add each member to our mapping (assuming the member is a string).
  for (const auto& item : maybe_members_list->GetList()) {
    // Members may not be a member of another set, and may not be an owner of
    // another set.
    if (item.is_string()) {
      std::string member = item.GetString();
      if (!members.contains(member) && !owners.contains(member)) {
        map_storage.emplace_back(member, *maybe_owner);
        members.insert(member);
      }
    }
  }
}

}  // namespace

std::unique_ptr<base::flat_map<std::string, std::string>>
FirstPartySetParser::ParsePreloadedSets(base::StringPiece raw_sets) {
  base::Optional<base::Value> maybe_value = base::JSONReader::Read(
      raw_sets, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!maybe_value.has_value())
    return nullptr;
  if (!maybe_value->is_list())
    return nullptr;

  std::vector<std::pair<std::string, std::string>> map_storage;
  base::flat_set<std::string> owners;
  base::flat_set<std::string> members;
  for (const auto& value : maybe_value->GetList()) {
    ParsePreloadedSet(value, map_storage, owners, members);
  }

  return std::make_unique<base::flat_map<std::string, std::string>>(
      std::move(map_storage));
}

}  // namespace network
