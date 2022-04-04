// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
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

namespace content {

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
const char kFirstPartySetPolicyReplacementsField[] = "replacements";
const char kFirstPartySetPolicyAdditionsField[] = "additions";

// Validates a single First-Party Set and parses it into a SingleSet.
// Note that this is intended for use *only* on sets that were received via the
// Component Updater or from enterprise policy, so this does not check
// assertions or versions. It rejects sets which are non-disjoint with
// previously-encountered sets (i.e. sets which have non-empty intersections
// with `elements`), and singleton sets (i.e. sets must have an owner and at
// least one valid member).
//
// Uses `elements` to check disjointness of sets; outputs the set as `out_set`;
// and augments `elements` to include the elements of the set that was parsed.
//
// Returns a nullopt if parsing and validation were successful, otherwise it
// returns an optional with an appropriate FirstPartySetParser::ParseError.
absl::optional<FirstPartySetParser::ParseError> ParseSet(
    const base::Value& value,
    base::flat_set<net::SchemefulSite>& elements,
    FirstPartySetParser::SingleSet& out_set) {
  if (!value.is_dict())
    return FirstPartySetParser::ParseError::kInvalidType;

  // Confirm that the set has an owner, and the owner is a string.
  const std::string* maybe_owner =
      value.GetDict().FindString(kFirstPartySetOwnerField);
  if (!maybe_owner)
    return FirstPartySetParser::ParseError::kInvalidType;

  absl::optional<net::SchemefulSite> canonical_owner =
      Canonicalize(std::move(*maybe_owner), false /* emit_errors */);
  if (!canonical_owner.has_value())
    return FirstPartySetParser::ParseError::kInvalidOrigin;

  // An owner may not be a member of another set.
  if (elements.contains(*canonical_owner))
    return FirstPartySetParser::ParseError::kNonDisjointSets;

  // Confirm that the members field is present, and is an array of strings.
  const base::Value* maybe_members_list =
      value.FindListKey(kFirstPartySetMembersField);
  if (!maybe_members_list)
    return FirstPartySetParser::ParseError::kInvalidType;

  if (maybe_members_list->GetListDeprecated().empty())
    return FirstPartySetParser::ParseError::kSingletonSet;

  std::vector<net::SchemefulSite> members;
  // Add each member to our mapping (assuming the member is a string).
  for (const auto& item : maybe_members_list->GetListDeprecated()) {
    // Members may not be a member of another set, and may not be an owner of
    // another set.
    if (!item.is_string())
      return FirstPartySetParser::ParseError::kInvalidType;
    absl::optional<net::SchemefulSite> member =
        Canonicalize(item.GetString(), false /* emit_errors */);
    if (!member.has_value())
      return FirstPartySetParser::ParseError::kInvalidOrigin;

    if (*member == *canonical_owner || base::Contains(members, member))
      return FirstPartySetParser::ParseError::kRepeatedDomain;

    if (elements.contains(*member))
      return FirstPartySetParser::ParseError::kNonDisjointSets;

    members.push_back(*member);
  }

  elements.insert(*canonical_owner);
  for (const auto& member : members) {
    elements.insert(member);
  }

  out_set = std::make_pair(*canonical_owner, members);
  return absl::nullopt;
}

// Parses each set in `policy_sets` by calling ParseSet on each one.
//
// Returns a PolicyParsingError if ParseSet returns an error, which contains the
// error that ParseSet returned along with the type of policy set that was being
// parsed and the index of the set that caused the error.
//
// If no call to ParseSet returns an error, `out_list` is populated with the
// list of parsed sets.
absl::optional<FirstPartySetParser::PolicyParsingError> GetPolicySetsFromList(
    const base::Value::List* policy_sets,
    base::flat_set<net::SchemefulSite>& elements,
    FirstPartySetParser::PolicySetType set_type,
    std::vector<FirstPartySetParser::SingleSet>& out_list) {
  if (!policy_sets) {
    out_list = {};
    return absl::nullopt;
  }

  std::vector<FirstPartySetParser::SingleSet> parsed_sets;
  for (int i = 0; i < static_cast<int>(policy_sets->size()); i++) {
    FirstPartySetParser::SingleSet out_set;
    if (absl::optional<FirstPartySetParser::ParseError> error =
            ParseSet((*policy_sets)[i], elements, out_set);
        error.has_value()) {
      return FirstPartySetParser::PolicyParsingError{error.value(), set_type,
                                                     i};
    }
    parsed_sets.push_back(out_set);
  }
  out_list = parsed_sets;
  return absl::nullopt;
}

}  // namespace

FirstPartySetParser::ParsedPolicySetLists::ParsedPolicySetLists(
    std::vector<FirstPartySetParser::SingleSet> replacement_list,
    std::vector<FirstPartySetParser::SingleSet> addition_list)
    : replacements(std::move(replacement_list)),
      additions(std::move(addition_list)) {}

FirstPartySetParser::ParsedPolicySetLists::ParsedPolicySetLists() = default;
FirstPartySetParser::ParsedPolicySetLists::ParsedPolicySetLists(
    FirstPartySetParser::ParsedPolicySetLists&&) = default;
FirstPartySetParser::ParsedPolicySetLists::ParsedPolicySetLists(
    const FirstPartySetParser::ParsedPolicySetLists&) = default;
FirstPartySetParser::ParsedPolicySetLists::~ParsedPolicySetLists() = default;

bool FirstPartySetParser::ParsedPolicySetLists::operator==(
    const FirstPartySetParser::ParsedPolicySetLists& other) const {
  return std::tie(replacements, additions) ==
         std::tie(other.replacements, other.additions);
}

FirstPartySetParser::SetsMap FirstPartySetParser::DeserializeFirstPartySets(
    base::StringPiece value) {
  if (value.empty())
    return {};

  std::unique_ptr<base::Value> value_deserialized =
      JSONStringValueDeserializer(value).Deserialize(
          nullptr /* error_code */, nullptr /* error_message */);
  if (!value_deserialized || !value_deserialized->is_dict())
    return {};

  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>> map;
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
      map.emplace_back(*maybe_owner, *maybe_owner);
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
    map.emplace_back(std::move(*maybe_member), std::move(*maybe_owner));
  }
  return map;
}

std::string FirstPartySetParser::SerializeFirstPartySets(
    const FirstPartySetParser::SetsMap& sets) {
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
  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>> map;
  base::flat_set<net::SchemefulSite> elements;
  for (std::string line; std::getline(input, line);) {
    base::StringPiece trimmed = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (trimmed.empty())
      continue;
    absl::optional<base::Value> maybe_value = base::JSONReader::Read(
        trimmed, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (!maybe_value.has_value())
      return {};
    FirstPartySetParser::SingleSet output;
    if (absl::optional<FirstPartySetParser::ParseError> error =
            ParseSet(*maybe_value, elements, output);
        error.has_value()) {
      if (*error == FirstPartySetParser::ParseError::kInvalidOrigin) {
        // Ignore sets that include an invalid domain (which might have been
        // caused by a PSL update), but don't let that break other sets.
        continue;
      }
      // Abort, something is wrong with the component.
      return {};
    }
    auto [owner, members] = output;
    map.emplace_back(owner, owner);
    for (net::SchemefulSite& member : members) {
      map.emplace_back(std::move(member), owner);
    }
  }
  return map;
}

absl::optional<FirstPartySetParser::PolicyParsingError>
FirstPartySetParser::ParseSetsFromEnterprisePolicy(
    const base::Value::Dict& policy,
    ParsedPolicySetLists* out_sets) {
  std::vector<SingleSet> parsed_replacements, parsed_additions;
  base::flat_set<net::SchemefulSite> elements;

  if (absl::optional<PolicyParsingError> error = GetPolicySetsFromList(
          policy.FindList(kFirstPartySetPolicyReplacementsField), elements,
          PolicySetType::kReplacement, parsed_replacements);
      error.has_value()) {
    return error.value();
  }

  if (absl::optional<PolicyParsingError> error = GetPolicySetsFromList(
          policy.FindList(kFirstPartySetPolicyAdditionsField), elements,
          PolicySetType::kAddition, parsed_additions);
      error.has_value()) {
    return error.value();
  }

  if (out_sets) {
    *out_sets = ParsedPolicySetLists(std::move(parsed_replacements),
                                     std::move(parsed_additions));
  }

  return absl::nullopt;
}

}  // namespace content
