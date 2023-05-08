// Copyright 2020 The Chromium Authors
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
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;
using ParseError = FirstPartySetsHandler::ParseError;
using ParseWarning = FirstPartySetsHandler::ParseWarning;
using Aliases = FirstPartySetParser::Aliases;
using SetsAndAliases = FirstPartySetParser::SetsAndAliases;
using SetsMap = FirstPartySetParser::SetsMap;

// Ensures that the string represents an origin that is non-opaque and HTTPS.
// Returns the registered domain.
base::expected<net::SchemefulSite, ParseErrorType> Canonicalize(
    base::StringPiece origin_string,
    bool emit_errors) {
  const url::Origin origin(url::Origin::Create(GURL(origin_string)));
  if (origin.opaque()) {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin " << origin_string
                 << " is not valid; ignoring.";
    }
    return base::unexpected(ParseErrorType::kInvalidOrigin);
  }
  if (origin.scheme() != "https") {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin " << origin_string
                 << " is not HTTPS; ignoring.";
    }
    return base::unexpected(ParseErrorType::kNonHttpsScheme);
  }
  absl::optional<net::SchemefulSite> site =
      net::SchemefulSite::CreateIfHasRegisterableDomain(origin);
  if (!site.has_value()) {
    if (emit_errors) {
      LOG(ERROR) << "First-Party Set origin " << origin_string
                 << " does not have a valid registered domain; ignoring.";
    }
    return base::unexpected(ParseErrorType::kInvalidDomain);
  }

  return site.value();
}

// Struct to hold metadata describing a particular "subset" during parsing.
struct SubsetDescriptor {
  std::string field_name;
  net::SiteType site_type;
  absl::optional<int> size_limit;
};

const char kFirstPartySetPrimaryField[] = "primary";
const char kFirstPartySetAssociatedSitesField[] = "associatedSites";
const char kFirstPartySetServiceSitesField[] = "serviceSites";
const char kCCTLDsField[] = "ccTLDs";
const char kFirstPartySetPolicyReplacementsField[] = "replacements";
const char kFirstPartySetPolicyAdditionsField[] = "additions";

bool IsSingletonSet(const std::vector<SetsMap::value_type>& set_entries,
                    const Aliases& aliases) {
  // There's no point in having a set with only one site and no aliases.
  return set_entries.size() + aliases.size() < 2;
}

// Parses a single base::Value into a net::SchemefulSite, and verifies that it
// is not already included in this set or any other.
base::expected<net::SchemefulSite, ParseErrorType> ParseSiteAndValidate(
    const base::Value& item,
    const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
        set_entries,
    const base::flat_set<net::SchemefulSite>& other_sets_sites,
    bool emit_errors) {
  if (!item.is_string())
    return base::unexpected(ParseErrorType::kInvalidType);

  const base::expected<net::SchemefulSite, ParseErrorType> maybe_site =
      Canonicalize(item.GetString(), emit_errors);
  if (!maybe_site.has_value())
    return base::unexpected(maybe_site.error());
  const net::SchemefulSite& site = *maybe_site;
  if (base::Contains(
          set_entries, site,
          &std::pair<net::SchemefulSite, net::FirstPartySetEntry>::first)) {
    return base::unexpected(ParseErrorType::kRepeatedDomain);
  }

  if (other_sets_sites.contains(site))
    return base::unexpected(ParseErrorType::kNonDisjointSets);

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
// than just the TLD, and adds a warning to `warnings`.
// Ignores any aliases provided for a representative site that is not in the
// First-Party Set we're currently parsing/validating, and adds a warning to
// `warnings`.
base::expected<Aliases, ParseError> ParseCctlds(
    const base::Value::Dict& set_declaration,
    const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
        set_entries,
    const base::flat_set<net::SchemefulSite>& elements,
    bool emit_errors,
    std::vector<ParseWarning>* warnings) {
  const base::Value::Dict* cctld_dict = set_declaration.FindDict(kCCTLDsField);
  if (!cctld_dict)
    return {};

  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>> aliases;
  for (const auto [site, site_alias_list] : *cctld_dict) {
    net::SchemefulSite site_as_schemeful_site((GURL(site)));
    if (!base::Contains(
            set_entries, site_as_schemeful_site,
            [](const auto& site_and_entry) { return site_and_entry.first; })) {
      if (warnings) {
        warnings->push_back(ParseWarning(
            ParseWarningType::kCctldKeyNotCanonical, {kCCTLDsField, site}));
      }
      continue;
    }

    const absl::optional<std::string> site_without_tld =
        RemoveTldFromSite(site_as_schemeful_site);
    if (!site_without_tld.has_value())
      continue;

    if (!site_alias_list.is_list())
      continue;

    const base::Value::List& site_aliases = site_alias_list.GetList();
    for (size_t i = 0; i < site_aliases.size(); ++i) {
      const base::expected<net::SchemefulSite, ParseErrorType> alias_or_error =
          ParseSiteAndValidate(site_aliases[i], set_entries, elements,
                               emit_errors);
      if (!alias_or_error.has_value()) {
        return base::unexpected(
            ParseError(alias_or_error.error(), {kCCTLDsField, site, static_cast<int>(i)}));
      }
      net::SchemefulSite alias = alias_or_error.value();
      const absl::optional<std::string> alias_site_without_tld =
          RemoveTldFromSite(alias);
      if (!alias_site_without_tld.has_value())
        continue;

      if (alias_site_without_tld != site_without_tld) {
        if (warnings) {
          warnings->push_back(
              ParseWarning(ParseWarningType::kAliasNotCctldVariant,
                           {kCCTLDsField, site, static_cast<int>(i)}));
        }
        continue;
      }
      aliases.emplace_back(std::move(alias), site_as_schemeful_site);
    }
  }

  return aliases;
}

// Parses a given optional subset, ensuring that it is disjoint from all other
// subsets in this set, and from all other sets that have previously been
// parsed.
base::expected<void, ParseError> ParseSubset(
    const base::Value::Dict& set_declaration,
    const net::SchemefulSite& primary,
    const SubsetDescriptor& descriptor,
    const base::flat_set<net::SchemefulSite>& other_sets_sites,
    bool emit_errors,
    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
        set_entries) {
  const base::Value* field_value = set_declaration.Find(descriptor.field_name);
  if (!field_value) {
    return base::ok();
  }
  if (!field_value->is_list()) {
    return base::unexpected(
        ParseError(ParseErrorType::kInvalidType, {descriptor.field_name}));
  }

  // Add each site to our mapping (after validating).
  uint32_t index = 0;
  for (const auto& item : field_value->GetList()) {
    base::expected<net::SchemefulSite, ParseErrorType> site_or_error =
        ParseSiteAndValidate(item, set_entries, other_sets_sites, emit_errors);
    if (!site_or_error.has_value()) {
      return base::unexpected(
          ParseError(site_or_error.error(),
                     {descriptor.field_name, static_cast<int>(index)}));
    }
    if (!descriptor.size_limit.has_value() ||
        static_cast<int>(index) < descriptor.size_limit.value()) {
      set_entries.emplace_back(
          site_or_error.value(),
          net::FirstPartySetEntry(
              primary, descriptor.site_type,
              descriptor.size_limit.has_value()
                  ? absl::make_optional(
                        net::FirstPartySetEntry::SiteIndex(index))
                  : absl::nullopt));
    }
    // Continue parsing even after we've reached the size limit (if there is
    // one), in order to surface malformed input domains as errors.
    ++index;
  }

  return base::ok();
}

// Validates a single First-Party Set and parses it into a SingleSet.
// Note that this is intended for use *only* on sets that were received via the
// Component Updater or from enterprise policy, so this does not check
// assertions or versions. It rejects sets which are non-disjoint with
// previously-encountered sets (i.e. sets which have non-empty intersections
// with `elements`), and singleton sets (i.e. sets must have a primary and at
// least one valid associated site).
//
// Uses `elements` to check disjointness of sets. The caller is expected to
// augment `elements` to include the elements of the set returned by this
// function (including any aliases).
//
// Returns the parsed set if parsing and validation were successful; otherwise,
// returns an appropriate ParseError.
//
// Outputs any warnings encountered during parsing to `warnings`,
// regardless of success/failure.
base::expected<SetsAndAliases, ParseError> ParseSet(
    const base::Value& value,
    bool exempt_from_limits,
    bool emit_errors,
    const base::flat_set<net::SchemefulSite>& elements,
    std::vector<ParseWarning>* warnings) {
  if (!value.is_dict())
    return base::unexpected(ParseError(ParseErrorType::kInvalidType, {}));

  const base::Value::Dict& set_declaration = value.GetDict();

  // Confirm that the set has a primary, and the primary is a string.
  const base::Value* primary_item =
      set_declaration.Find(kFirstPartySetPrimaryField);
  if (!primary_item) {
    return base::unexpected(
        ParseError(ParseErrorType::kInvalidType, {kFirstPartySetPrimaryField}));
  }

  base::expected<net::SchemefulSite, ParseErrorType> primary_or_error =
      ParseSiteAndValidate(*primary_item, /*set_entries=*/{}, elements,
                           emit_errors);
  if (!primary_or_error.has_value()) {
    return base::unexpected(
        ParseError(primary_or_error.error(), {kFirstPartySetPrimaryField}));
  }
  const net::SchemefulSite& primary = primary_or_error.value();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries(
          {{primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                             absl::nullopt)}});

  for (const SubsetDescriptor& descriptor : {
           SubsetDescriptor{
               .field_name = kFirstPartySetAssociatedSitesField,
              .site_type = net::SiteType::kAssociated,
              .size_limit =
                   exempt_from_limits
                       ? absl::nullopt
                       : absl::make_optional(
                             features::kFirstPartySetsMaxAssociatedSites.Get()),
           },
           {
               .field_name = kFirstPartySetServiceSitesField,
              .site_type = net::SiteType::kService,
              .size_limit = absl::nullopt,
           },
       }) {
    if (base::expected<void, ParseError> result =
            ParseSubset(set_declaration, primary, descriptor, elements,
                        emit_errors, set_entries);
        !result.has_value()) {
      return base::unexpected(result.error());
    }
  }

  const base::expected<Aliases, ParseError> aliases_or_error = ParseCctlds(
      set_declaration, set_entries, elements, emit_errors, warnings);
  if (!aliases_or_error.has_value())
    return base::unexpected(aliases_or_error.error());
  const Aliases& aliases = aliases_or_error.value();

  if (IsSingletonSet(set_entries, aliases)) {
    return base::unexpected(ParseError(ParseErrorType::kSingletonSet,
                                       {kFirstPartySetAssociatedSitesField}));
  }

  return std::make_pair(FirstPartySetParser::SingleSet(set_entries), aliases);
}

const char* SetTypeToString(FirstPartySetParser::PolicySetType set_type) {
  switch (set_type) {
    case FirstPartySetParser::PolicySetType::kReplacement:
      return kFirstPartySetPolicyReplacementsField;
    case FirstPartySetParser::PolicySetType::kAddition:
      return kFirstPartySetPolicyAdditionsField;
  }
}

// Returns the parsed sets if successful; otherwise returns the first error.
// Stores any warnings encountered when parsing in the `warnings` out-parameter.
base::expected<std::vector<FirstPartySetParser::SingleSet>, ParseError>
GetPolicySetsFromList(const base::Value::List* policy_sets,
                      base::flat_set<net::SchemefulSite>& elements,
                      FirstPartySetParser::PolicySetType set_type,
                      std::vector<ParseWarning>& warnings) {
  if (!policy_sets) {
    return {};
  }

  std::vector<FirstPartySetParser::SingleSet> parsed_sets;
  size_t previous_size = warnings.size();
  for (int i = 0; i < static_cast<int>(policy_sets->size()); i++) {
    base::expected<SetsAndAliases, ParseError> parsed =
        ParseSet((*policy_sets)[i], /*exempt_from_limits=*/true,
                 /*emit_errors=*/false, elements, &warnings);
    for (auto it = warnings.begin() + previous_size; it != warnings.end();
         it++) {
      it->PrependPath({SetTypeToString(set_type), i});
    }
    if (!parsed.has_value()) {
      ParseError error = parsed.error();
      error.PrependPath({SetTypeToString(set_type), i});
      return base::unexpected(error);
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
    for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
             site_and_entry : set) {
      CHECK(elements.insert(site_and_entry.first).second);
    }
    parsed_sets.push_back(set);
    previous_size = warnings.size();
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

absl::optional<net::SchemefulSite>
FirstPartySetParser::CanonicalizeRegisteredDomain(
    const base::StringPiece origin_string,
    bool emit_errors) {
  base::expected<net::SchemefulSite, ParseErrorType> maybe_site =
      Canonicalize(origin_string, emit_errors);
  if (!maybe_site.has_value()) {
    return absl::nullopt;
  }
  return maybe_site.value();
}

SetsAndAliases FirstPartySetParser::ParseSetsFromStream(std::istream& input,
                                                        bool emit_errors,
                                                        bool emit_metrics) {
  std::vector<SetsMap::value_type> sets;
  std::vector<Aliases::value_type> aliases;
  base::flat_set<SetsMap::key_type> elements;
  int successfully_parsed_sets = 0;
  int nonfatal_errors = 0;
  for (std::string line; std::getline(input, line);) {
    base::StringPiece trimmed = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (trimmed.empty()) {
      continue;
    }
    absl::optional<base::Value> maybe_value = base::JSONReader::Read(
        trimmed, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (!maybe_value.has_value()) {
      if (emit_metrics) {
        base::UmaHistogramBoolean(
            "Cookie.FirstPartySets.ProcessedEntireComponent", false);
      }
      return {};
    }
    base::expected<SetsAndAliases, ParseError> parsed = ParseSet(
        *maybe_value, /*exempt_from_limits=*/false, emit_errors, elements,
        /*warnings=*/nullptr);
    if (!parsed.has_value()) {
      if (parsed.error().type() == ParseErrorType::kInvalidDomain) {
        // Ignore sets that include an invalid domain (which might have been
        // caused by a PSL update), but don't let that break other sets.
        nonfatal_errors++;
        continue;
      }
      // Abort, something is wrong with the component.
      if (emit_metrics) {
        base::UmaHistogramBoolean(
            "Cookie.FirstPartySets.ProcessedEntireComponent", false);
      }
      return {};
    }

    for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
             site_and_entry : parsed->first) {
      const net::SchemefulSite& site = site_and_entry.first;
      CHECK(elements.insert(site).second);
    }
    for (const std::pair<net::SchemefulSite, net::SchemefulSite>&
             alias_and_canonical : parsed->second) {
      const net::SchemefulSite& alias = alias_and_canonical.first;
      CHECK(elements.insert(alias).second);
    }

    base::ranges::move(parsed.value().first, std::back_inserter(sets));
    base::ranges::move(parsed.value().second, std::back_inserter(aliases));
    successfully_parsed_sets++;
  }
  if (emit_metrics) {
    base::UmaHistogramBoolean("Cookie.FirstPartySets.ProcessedEntireComponent",
                              true);
    base::UmaHistogramCounts1000(
        "Cookie.FirstPartySets.ComponentSetsParsedSuccessfully",
        successfully_parsed_sets);
    base::UmaHistogramCounts1000(
        "Cookie.FirstPartySets.ComponentSetsNonfatalErrors", nonfatal_errors);
  }

  return std::make_pair(sets, aliases);
}

FirstPartySetParser::PolicyParseResult
FirstPartySetParser::ParseSetsFromEnterprisePolicy(
    const base::Value::Dict& policy) {
  std::vector<ParseWarning> warnings;
  const auto get_set_lists =
      [&]() -> base::expected<ParsedPolicySetLists,
                              FirstPartySetsHandler::ParseError> {
    base::flat_set<net::SchemefulSite> elements;
    base::expected<std::vector<SingleSet>, ParseError> parsed_replacements =
        GetPolicySetsFromList(
            policy.FindList(kFirstPartySetPolicyReplacementsField), elements,
            PolicySetType::kReplacement, warnings);
    if (!parsed_replacements.has_value()) {
      return base::unexpected(parsed_replacements.error());
    }
    base::expected<std::vector<SingleSet>, ParseError> parsed_additions =
        GetPolicySetsFromList(
            policy.FindList(kFirstPartySetPolicyAdditionsField), elements,
            PolicySetType::kAddition, warnings);
    if (!parsed_additions.has_value()) {
      return base::unexpected(parsed_additions.error());
    }
    return ParsedPolicySetLists(std::move(parsed_replacements.value()),
                                std::move(parsed_additions.value()));
  };
  return FirstPartySetParser::PolicyParseResult(get_set_lists(), warnings);
}

}  // namespace content
