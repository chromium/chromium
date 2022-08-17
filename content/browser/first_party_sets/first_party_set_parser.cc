// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <cstdint>
#include <iterator>
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
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ParseError = FirstPartySetParser::ParseError;
using Aliases = FirstPartySetParser::Aliases;
using SetsAndAliases = FirstPartySetParser::SetsAndAliases;
using SetsMap = FirstPartySetParser::SetsMap;

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
const char kCCTLDsField[] = "ccTLDs";
const char kFirstPartySetPolicyReplacementsField[] = "replacements";
const char kFirstPartySetPolicyAdditionsField[] = "additions";

// Parses a single base::Value into a net::SchemefulSite, and verifies that it
// is not already included in this set or any other.
base::expected<net::SchemefulSite, ParseError> ParseSiteAndValidate(
    const base::Value& item,
    const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
        set_entries,
    const base::flat_set<net::SchemefulSite>& other_sets_sites) {
  if (!item.is_string())
    return base::unexpected(ParseError::kInvalidType);

  const absl::optional<net::SchemefulSite> maybe_site =
      Canonicalize(item.GetString(), false /* emit_errors */);
  if (!maybe_site.has_value())
    return base::unexpected(ParseError::kInvalidOrigin);

  const net::SchemefulSite& site = *maybe_site;
  if (base::ranges::any_of(
          set_entries,
          [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
                  site_and_entry) { return site_and_entry.first == site; })) {
    return base::unexpected(ParseError::kRepeatedDomain);
  }

  if (other_sets_sites.contains(site))
    return base::unexpected(ParseError::kNonDisjointSets);

  return site;
}

// Removes the TLD from a SchemefulSite, if possible. (It is not possible if the
// site has no final subcomponent.)
absl::optional<std::string> RemoveTldFromSite(const net::SchemefulSite& site) {
  const size_t tld_length = net::registry_controlled_domains::GetRegistryLength(
      site.GetURL(),
      net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (tld_length == 0)
    return absl::nullopt;
  const std::string serialized = site.Serialize();
  return serialized.substr(0, serialized.size() - tld_length);
}

// Parses the optional ccTLDs field, if present. If absent, this is a no-op.
// Returns any error encountered while parsing the strings into SchemefulSites.
//
// Ignores any aliases that differ from their canonical representative by more
// than just the TLD. Ignores any aliases provided for a representative site
// that is not in the First-Party Set we're currently parsing/validating.
base::expected<Aliases, ParseError> ParseCctlds(
    const base::Value::Dict& set_declaration,
    const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
        set_entries,
    const base::flat_set<net::SchemefulSite>& elements) {
  const base::Value::Dict* cctld_dict = set_declaration.FindDict(kCCTLDsField);
  if (!cctld_dict)
    return {};

  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>> aliases;
  for (const auto& site_and_entry : set_entries) {
    const std::string site = site_and_entry.first.Serialize();
    const absl::optional<std::string> site_without_tld =
        RemoveTldFromSite(site_and_entry.first);
    if (!site_without_tld.has_value())
      continue;

    const base::Value::List* cctld_list = cctld_dict->FindList(site);
    if (!cctld_list)
      continue;
    for (const base::Value& item : *cctld_list) {
      const base::expected<net::SchemefulSite, ParseError> alias_or_error =
          ParseSiteAndValidate(item, set_entries, elements);
      if (!alias_or_error.has_value())
        return base::unexpected(alias_or_error.error());

      const net::SchemefulSite alias = alias_or_error.value();
      const absl::optional<std::string> alias_site_without_tld =
          RemoveTldFromSite(alias);
      if (!alias_site_without_tld.has_value())
        continue;

      if (alias_site_without_tld != site_without_tld)
        continue;

      aliases.emplace_back(alias, site_and_entry.first);
    }
  }

  return aliases;
}

// Validates a single First-Party Set and parses it into a SingleSet.
// Note that this is intended for use *only* on sets that were received via the
// Component Updater or from enterprise policy, so this does not check
// assertions or versions. It rejects sets which are non-disjoint with
// previously-encountered sets (i.e. sets which have non-empty intersections
// with `elements`), and singleton sets (i.e. sets must have an owner and at
// least one valid member).
//
// Uses `elements` to check disjointness of sets; augments `elements` to include
// the elements of the set that was parsed.
//
// Returns the parsed set if parsing and validation were successful; otherwise,
// returns an appropriate FirstPartySetParser::ParseError.
base::expected<SetsAndAliases, ParseError> ParseSet(
    const base::Value& value,
    bool keep_indices,
    base::flat_set<net::SchemefulSite>& elements) {
  if (!value.is_dict())
    return base::unexpected(ParseError::kInvalidType);

  const base::Value::Dict& set_declaration = value.GetDict();

  // Confirm that the set has an owner, and the owner is a string.
  const base::Value* primary_item =
      set_declaration.Find(kFirstPartySetOwnerField);
  if (!primary_item)
    return base::unexpected(ParseError::kInvalidType);

  base::expected<net::SchemefulSite, ParseError> primary_or_error =
      ParseSiteAndValidate(*primary_item, /*set_entries=*/{}, elements);
  if (!primary_or_error.has_value()) {
    return base::unexpected(primary_or_error.error());
  }
  const net::SchemefulSite& primary = primary_or_error.value();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries(
          {{primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                             absl::nullopt)}});

  // Confirm that the members field is present, and is an array of strings.
  const base::Value::List* maybe_members_list =
      set_declaration.FindList(kFirstPartySetMembersField);
  if (!maybe_members_list)
    return base::unexpected(ParseError::kInvalidType);

  if (maybe_members_list->empty())
    return base::unexpected(ParseError::kSingletonSet);

  // Add each member to our mapping (after validating).
  uint32_t index = 0;
  for (const auto& item : *maybe_members_list) {
    base::expected<net::SchemefulSite, ParseError> site_or_error =
        ParseSiteAndValidate(item, set_entries, elements);
    if (!site_or_error.has_value()) {
      return base::unexpected(site_or_error.error());
    }
    set_entries.emplace_back(
        site_or_error.value(),
        net::FirstPartySetEntry(
            primary, net::SiteType::kAssociated,
            keep_indices
                ? absl::make_optional(net::FirstPartySetEntry::SiteIndex(index))
                : absl::nullopt));
    ++index;
  }

  const base::expected<Aliases, ParseError> aliases_or_error =
      ParseCctlds(set_declaration, set_entries, elements);
  if (!aliases_or_error.has_value())
    return base::unexpected(aliases_or_error.error());

  const Aliases& aliases = aliases_or_error.value();

  for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
           site_and_entry : set_entries) {
    bool inserted = elements.insert(site_and_entry.first).second;
    DCHECK(inserted);
  }
  for (const std::pair<net::SchemefulSite, net::SchemefulSite>&
           alias_and_canonical : aliases) {
    bool inserted = elements.insert(alias_and_canonical.first).second;
    DCHECK(inserted);
  }

  return std::make_pair(FirstPartySetParser::SingleSet(set_entries), aliases);
}

// Parses each set in `policy_sets` by calling ParseSet on each one.
//
// Returns the parsed sets if successful; otherwise returns the first error.
base::expected<std::vector<FirstPartySetParser::SingleSet>,
               FirstPartySetParser::PolicyParsingError>
GetPolicySetsFromList(const base::Value::List* policy_sets,
                      base::flat_set<net::SchemefulSite>& elements,
                      FirstPartySetParser::PolicySetType set_type) {
  if (!policy_sets) {
    return {};
  }

  std::vector<FirstPartySetParser::SingleSet> parsed_sets;
  for (int i = 0; i < static_cast<int>(policy_sets->size()); i++) {
    base::expected<SetsAndAliases, ParseError> parsed =
        ParseSet((*policy_sets)[i], /*keep_indices=*/false, elements);
    if (!parsed.has_value()) {
      return base::unexpected(
          FirstPartySetParser::PolicyParsingError{parsed.error(), set_type, i});
    }
    SetsMap& set = parsed.value().first;
    if (!parsed.value().second.empty()) {
      std::vector<SetsMap::value_type> alias_entries;
      for (const auto& alias : parsed.value().second) {
        alias_entries.emplace_back(alias.first, set.find(alias.second)->second);
      }
      set.insert(std::make_move_iterator(alias_entries.begin()),
                 std::make_move_iterator(alias_entries.end()));
    }
    parsed_sets.push_back(set);
  }
  return parsed_sets;
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

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> map;
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
      map.emplace_back(*maybe_owner, net::FirstPartySetEntry(
                                         *maybe_owner, net::SiteType::kPrimary,
                                         absl::nullopt));
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
    // TODO(https://crbug.com/1219656): preserve ordering information when
    // persisting set info.
    map.emplace_back(
        std::move(*maybe_member),
        net::FirstPartySetEntry(std::move(*maybe_owner),
                                net::SiteType::kAssociated, absl::nullopt));
  }
  return map;
}

std::string FirstPartySetParser::SerializeFirstPartySets(
    const FirstPartySetParser::SetsMap& sets) {
  base::DictionaryValue dict;
  for (const auto& it : sets) {
    std::string maybe_member = it.first.Serialize();
    std::string owner = it.second.primary().Serialize();
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

SetsAndAliases FirstPartySetParser::ParseSetsFromStream(std::istream& input) {
  std::vector<SetsMap::value_type> sets;
  std::vector<Aliases::value_type> aliases;
  base::flat_set<SetsMap::key_type> elements;
  for (std::string line; std::getline(input, line);) {
    base::StringPiece trimmed = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (trimmed.empty())
      continue;
    absl::optional<base::Value> maybe_value = base::JSONReader::Read(
        trimmed, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (!maybe_value.has_value())
      return {};
    base::expected<SetsAndAliases, ParseError> parsed =
        ParseSet(*maybe_value, /*keep_indices=*/true, elements);
    if (!parsed.has_value()) {
      if (parsed.error() == ParseError::kInvalidOrigin) {
        // Ignore sets that include an invalid domain (which might have been
        // caused by a PSL update), but don't let that break other sets.
        continue;
      }
      // Abort, something is wrong with the component.
      return {};
    }

    base::ranges::move(parsed.value().first, std::back_inserter(sets));
    base::ranges::move(parsed.value().second, std::back_inserter(aliases));
  }
  return std::make_pair(sets, aliases);
}

base::expected<FirstPartySetParser::ParsedPolicySetLists,
               FirstPartySetParser::PolicyParsingError>
FirstPartySetParser::ParseSetsFromEnterprisePolicy(
    const base::Value::Dict& policy) {
  base::flat_set<net::SchemefulSite> elements;

  base::expected<std::vector<SingleSet>, PolicyParsingError>
      parsed_replacements = GetPolicySetsFromList(
          policy.FindList(kFirstPartySetPolicyReplacementsField), elements,
          PolicySetType::kReplacement);
  if (!parsed_replacements.has_value()) {
    return base::unexpected(parsed_replacements.error());
  }

  base::expected<std::vector<SingleSet>, PolicyParsingError> parsed_additions =
      GetPolicySetsFromList(policy.FindList(kFirstPartySetPolicyAdditionsField),
                            elements, PolicySetType::kAddition);
  if (!parsed_additions.has_value()) {
    return base::unexpected(parsed_additions.error());
  }

  return ParsedPolicySetLists(std::move(parsed_replacements.value()),
                              std::move(parsed_additions.value()));
}

}  // namespace content
