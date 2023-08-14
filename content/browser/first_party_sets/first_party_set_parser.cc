// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
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

constexpr char kFirstPartySetPrimaryField[] = "primary";
constexpr char kFirstPartySetAssociatedSitesField[] = "associatedSites";
constexpr char kFirstPartySetServiceSitesField[] = "serviceSites";
constexpr char kCCTLDsField[] = "ccTLDs";
constexpr char kFirstPartySetPolicyReplacementsField[] = "replacements";
constexpr char kFirstPartySetPolicyAdditionsField[] = "additions";

const char* SetTypeToString(FirstPartySetParser::PolicySetType set_type) {
  switch (set_type) {
    case FirstPartySetParser::PolicySetType::kReplacement:
      return kFirstPartySetPolicyReplacementsField;
    case FirstPartySetParser::PolicySetType::kAddition:
      return kFirstPartySetPolicyAdditionsField;
  }
}

// Class representing the results of validating a given site.
class ValidateSiteResult {
 public:
  ValidateSiteResult(net::SchemefulSite site, bool modified_host)
      : ValidateSiteResult(std::move(site),
                           absl::nullopt,
                           /*modified_host=*/modified_host) {}

  explicit ValidateSiteResult(ParseErrorType error_type)
      : ValidateSiteResult(absl::nullopt, error_type, /*modified_host=*/false) {
  }

  ValidateSiteResult(net::SchemefulSite site, ParseErrorType error_type)
      : ValidateSiteResult(absl::make_optional(std::move(site)),
                           absl::make_optional(error_type),
                           /*modified_host=*/false) {
    // If we have both a site and an error, the error must be because the site
    // didn't have a registerable domain (but we were still able to parse it).
    CHECK_EQ(error_type_.value(), ParseErrorType::kInvalidDomain);
  }

  bool has_site() const { return site_.has_value(); }
  bool has_error() const { return error_type_.has_value(); }

  const net::SchemefulSite& site() const { return site_.value(); }
  ParseErrorType error_type() const { return error_type_.value(); }
  bool modified_host() const { return modified_host_; }

 private:
  ValidateSiteResult(absl::optional<net::SchemefulSite> site,
                     absl::optional<ParseErrorType> error_type,
                     bool modified_host)
      : site_(std::move(site)),
        error_type_(error_type),
        modified_host_(modified_host) {}

  const absl::optional<net::SchemefulSite> site_ = absl::nullopt;
  const absl::optional<ParseErrorType> error_type_ = absl::nullopt;
  const bool modified_host_ = false;
};

bool IsFatalError(ParseErrorType error_type) {
  switch (error_type) {
    case FirstPartySetsHandler::ParseErrorType::kInvalidType:
    case FirstPartySetsHandler::ParseErrorType::kInvalidOrigin:
    case FirstPartySetsHandler::ParseErrorType::kNonHttpsScheme:
    case FirstPartySetsHandler::ParseErrorType::kNonDisjointSets:
    case FirstPartySetsHandler::ParseErrorType::kRepeatedDomain:
      // Fatal errors indicate that something is wrong with the data source,
      // since it could never have been valid.
      return true;
    case FirstPartySetsHandler::ParseErrorType::kInvalidDomain:
    case FirstPartySetsHandler::ParseErrorType::kSingletonSet:
      // Nonfatal errors may arise over time (as the Public Suffix List changes,
      // or due to other nonfatal errors), so they don't indicate a problem with
      // the data source.
      return false;
  }
}

// Struct to hold metadata describing a particular "subset" during parsing.
struct SubsetDescriptor {
  std::string field_name;
  net::SiteType site_type;
  absl::optional<int> size_limit;
};

bool IsSingletonSet(const std::vector<SetsMap::value_type>& set_entries,
                    const Aliases& aliases) {
  // There's no point in having a set with only one site and no aliases.
  return set_entries.size() + aliases.size() < 2;
}

// Removes the TLD from a SchemefulSite, if possible. (It is not possible if
// the site has no final subcomponent.)
absl::optional<std::string> RemoveTldFromSite(const net::SchemefulSite& site) {
  const size_t tld_length = net::registry_controlled_domains::GetRegistryLength(
      site.GetURL(),
      net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (tld_length == 0) {
    return absl::nullopt;
  }
  const std::string serialized = site.Serialize();
  return serialized.substr(0, serialized.size() - tld_length);
}

class ParseContext {
 public:
  ParseContext(bool emit_errors, bool exempt_from_limits)
      : emit_errors_(emit_errors), exempt_from_limits_(exempt_from_limits) {}

  ParseContext(const ParseContext&) = delete;
  ParseContext& operator=(const ParseContext&) = delete;

  ~ParseContext() = default;

  // Ensures that the string represents an origin that is non-opaque and HTTPS.
  // Returns the registered domain.
  ValidateSiteResult Canonicalize(base::StringPiece origin_string) const {
    const url::Origin origin(url::Origin::Create(GURL(origin_string)));
    if (origin.opaque()) {
      if (emit_errors_) {
        LOG(ERROR) << "First-Party Set origin " << origin_string
                   << " is not valid; ignoring.";
      }
      return ValidateSiteResult(ParseErrorType::kInvalidOrigin);
    }
    if (origin.scheme() != "https") {
      if (emit_errors_) {
        LOG(ERROR) << "First-Party Set origin " << origin_string
                   << " is not HTTPS; ignoring.";
      }
      return ValidateSiteResult(ParseErrorType::kNonHttpsScheme);
    }
    absl::optional<net::SchemefulSite> site =
        net::SchemefulSite::CreateIfHasRegisterableDomain(origin);
    if (!site.has_value()) {
      if (emit_errors_) {
        LOG(ERROR) << "First-Party Set origin " << origin_string
                   << " does not have a valid registered domain; ignoring.";
      }
      return ValidateSiteResult(net::SchemefulSite(origin),
                                ParseErrorType::kInvalidDomain);
    }

    bool modified_host = origin.host() != site->GetURL().host();
    return ValidateSiteResult(std::move(site).value(), modified_host);
  }

  // Validates a single First-Party Set and parses it into a SingleSet.
  // Note that this is intended for use *only* on sets that were received via
  // the Component Updater or from enterprise policy, so this does not check
  // assertions or versions. It rejects sets which are non-disjoint with
  // previously-encountered sets (i.e. sets which have non-empty intersections
  // with `elements`), and singleton sets (i.e. sets must have a primary and at
  // least one valid associated site).
  //
  // Uses `elements_` to check disjointness of sets. The caller is expected to
  // update this context via `AddSet` after this method terminates.
  //
  // Returns the parsed set if parsing and validation were successful;
  // otherwise, returns an appropriate ParseError.
  base::expected<SetsAndAliases, ParseError> ParseSet(
      const base::Value& value) const {
    if (!value.is_dict()) {
      return base::unexpected(ParseError(ParseErrorType::kInvalidType, {}));
    }

    const base::Value::Dict& set_declaration = value.GetDict();

    // Confirm that the set has a primary, and the primary is a string.
    const base::Value* primary_item =
        set_declaration.Find(kFirstPartySetPrimaryField);
    if (!primary_item) {
      return base::unexpected(ParseError(ParseErrorType::kInvalidType,
                                         {kFirstPartySetPrimaryField}));
    }

    ValidateSiteResult primary_result =
        ParseSiteAndValidate(*primary_item, /*set_entries=*/{});
    if (primary_result.has_error()) {
      // Primaries that exhibit any error (fatal or nonfatal) invalidate the
      // whole set.
      return base::unexpected(ParseError(primary_result.error_type(),
                                         {kFirstPartySetPrimaryField}));
    }
    const net::SchemefulSite& primary = primary_result.site();

    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
        set_entries(
            {{primary, net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                               absl::nullopt)}});

    for (const SubsetDescriptor& descriptor : {
             SubsetDescriptor{
                 .field_name = kFirstPartySetAssociatedSitesField,
                 .site_type = net::SiteType::kAssociated,
                 .size_limit =
                     exempt_from_limits_
                         ? absl::nullopt
                         : absl::make_optional(
                               features::kFirstPartySetsMaxAssociatedSites
                                   .Get()),
             },
             {
                 .field_name = kFirstPartySetServiceSitesField,
                .site_type = net::SiteType::kService,
                .size_limit = absl::nullopt,
             },
         }) {
      RETURN_IF_ERROR(
          ParseSubset(set_declaration, primary, descriptor, set_entries));
    }

    ASSIGN_OR_RETURN(const Aliases& aliases,
                     ParseCctlds(set_declaration, set_entries));

    if (IsSingletonSet(set_entries, aliases)) {
      return base::unexpected(ParseError(ParseErrorType::kSingletonSet,
                                         {kFirstPartySetAssociatedSitesField}));
    }

    return std::make_pair(FirstPartySetParser::SingleSet(set_entries), aliases);
  }

  // Returns the parsed sets if successful; otherwise returns the first error.
  base::expected<std::vector<FirstPartySetParser::SingleSet>, ParseError>
  GetPolicySetsFromList(const base::Value::List* policy_sets,
                        FirstPartySetParser::PolicySetType set_type) {
    if (!policy_sets) {
      return {};
    }

    std::vector<FirstPartySetParser::SingleSet> parsed_sets;
    size_t previous_size = warnings_.size();
    for (int i = 0; i < static_cast<int>(policy_sets->size()); i++) {
      base::expected<SetsAndAliases, ParseError> parsed =
          ParseSet((*policy_sets)[i]);
      for (auto it = warnings_.begin() + previous_size; it != warnings_.end();
           it++) {
        it->PrependPath({SetTypeToString(set_type), i});
      }
      if (!parsed.has_value()) {
        if (!IsFatalError(parsed.error().type())) {
          continue;
        }
        ParseError error = parsed.error();
        error.PrependPath({SetTypeToString(set_type), i});
        return base::unexpected(error);
      }

      SetsMap& set = parsed.value().first;
      if (!parsed.value().second.empty()) {
        std::vector<SetsMap::value_type> alias_entries;
        for (const auto& alias : parsed.value().second) {
          alias_entries.emplace_back(alias.first,
                                     set.find(alias.second)->second);
        }
        set.insert(std::make_move_iterator(alias_entries.begin()),
                   std::make_move_iterator(alias_entries.end()));
      }
      AddSet(SetsAndAliases(set, {}));
      parsed_sets.push_back(std::move(set));
      previous_size = warnings_.size();
    }
    return parsed_sets;
  }

  // Updates the context to include the given set and aliases.
  //
  // The given set and aliases must be disjoint from everything previously added
  // to the context.
  void AddSet(const SetsAndAliases& set_and_aliases) {
    for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
             site_and_entry : set_and_aliases.first) {
      const net::SchemefulSite& site = site_and_entry.first;
      CHECK(elements_.insert(site).second);
    }
    for (const std::pair<net::SchemefulSite, net::SchemefulSite>&
             alias_and_canonical : set_and_aliases.second) {
      const net::SchemefulSite& alias = alias_and_canonical.first;
      CHECK(elements_.insert(alias).second);
    }
  }

  // Removes invalid site entries and aliases, and fixes up any lingering
  // singletons. Modifies the data in-place.
  void PostProcessSets(std::vector<SetsMap::value_type>& sets,
                       std::vector<Aliases::value_type>& aliases) const {
    if (invalid_keys_.empty()) {
      return;
    }

    base::flat_set<net::SchemefulSite> possible_singletons;
    // Erase invalid aliases, and collect canonical sites that are primaries
    // and might become singletons.
    base::EraseIf(
        aliases,
        [&](const std::pair<net::SchemefulSite, net::SchemefulSite>& pair)
            -> bool {
          return IsInvalidAlias(pair, possible_singletons, sets);
        });

    // Erase invalid members/primaries, and collect more possible-singletons.
    base::EraseIf(
        sets,
        [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>& pair)
            -> bool { return IsInvalidEntry(pair, &possible_singletons); });

    if (possible_singletons.empty()) {
      return;
    }

    // Since we just removed some keys, we have to double-check that there are
    // no singleton sets.
    for (const std::pair<net::SchemefulSite, net::FirstPartySetEntry>& pair :
         sets) {
      const net::SchemefulSite& site = pair.first;
      const net::FirstPartySetEntry& entry = pair.second;
      if (site == entry.primary()) {
        // Skip primaries, they don't count as their own members.
        continue;
      }
      // Found at least one member for this primary, so it isn't a
      // singleton.
      possible_singletons.erase(entry.primary());
    }
    // Any canonical site that has at least one alias is not a singleton.
    for (const std::pair<net::SchemefulSite, net::SchemefulSite>& pair :
         aliases) {
      possible_singletons.erase(pair.second);
    }

    if (possible_singletons.empty()) {
      return;
    }

    base::EraseIf(
        sets,
        [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>& pair)
            -> bool { return possible_singletons.contains(pair.first); });
  }

  // Removes invalid site entries and fixes up any lingering singletons.
  // Modifies the lists in-place.
  void PostProcessSetLists(
      base::expected<FirstPartySetParser::ParsedPolicySetLists,
                     FirstPartySetsHandler::ParseError>& lists_or_error) const {
    if (!lists_or_error.has_value() || invalid_keys_.empty()) {
      return;
    }

    FirstPartySetParser::ParsedPolicySetLists& lists = lists_or_error.value();

    // Erase invalid members/primaries.
    const auto is_invalid_entry =
        [&](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>& pair)
        -> bool {
      return IsInvalidEntry(pair, /*possible_singletons=*/nullptr);
    };
    for (auto& set : lists.additions) {
      base::EraseIf(set, is_invalid_entry);
    }
    for (auto& set : lists.replacements) {
      base::EraseIf(set, is_invalid_entry);
    }

    // Since we just removed some keys, we have to double-check that there are
    // no singleton sets.
    const auto is_singleton = [](const FirstPartySetParser::SingleSet& set) {
      return set.size() <= 1;
    };
    base::EraseIf(lists.additions, is_singleton);
    base::EraseIf(lists.replacements, is_singleton);
  }

  std::vector<ParseWarning>& warnings() { return warnings_; }

 private:
  // Parses a given optional subset, ensuring that it is disjoint from all other
  // subsets in this set, and from all other sets that have previously been
  // parsed.
  base::expected<void, ParseError> ParseSubset(
      const base::Value::Dict& set_declaration,
      const net::SchemefulSite& primary,
      const SubsetDescriptor& descriptor,
      std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
          set_entries) const {
    const base::Value* field_value =
        set_declaration.Find(descriptor.field_name);
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
      ValidateSiteResult site_result = ParseSiteAndValidate(item, set_entries);
      if (site_result.has_error()) {
        if (!IsFatalError(site_result.error_type())) {
          ++index;
          continue;
        }
        return base::unexpected(
            ParseError(site_result.error_type(),
                       {descriptor.field_name, static_cast<int>(index)}));
      }
      if (!descriptor.size_limit.has_value() ||
          static_cast<int>(index) < descriptor.size_limit.value()) {
        set_entries.emplace_back(
            site_result.site(),
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

  // Parses the optional ccTLDs field, if present. If absent, this is a no-op.
  // Returns any error encountered while parsing the strings into
  // SchemefulSites.
  //
  // Ignores any aliases that differ from their canonical representative by more
  // than just the TLD.
  // Ignores any aliases provided for a representative site that is not in the
  // First-Party Set we're currently parsing/validating.
  base::expected<Aliases, ParseError> ParseCctlds(
      const base::Value::Dict& set_declaration,
      const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
          set_entries) const {
    const base::Value::Dict* cctld_dict =
        set_declaration.FindDict(kCCTLDsField);
    if (!cctld_dict) {
      return {};
    }

    std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>> aliases;
    for (const auto [site, site_alias_list] : *cctld_dict) {
      net::SchemefulSite site_as_schemeful_site((GURL(site)));
      if (!base::Contains(set_entries, site_as_schemeful_site,
                          [](const auto& site_and_entry) {
                            return site_and_entry.first;
                          })) {
        warnings_.push_back(ParseWarning(
            ParseWarningType::kCctldKeyNotCanonical, {kCCTLDsField, site}));
        continue;
      }

      const absl::optional<std::string> site_without_tld =
          RemoveTldFromSite(site_as_schemeful_site);
      if (!site_without_tld.has_value()) {
        continue;
      }

      if (!site_alias_list.is_list()) {
        continue;
      }

      const base::Value::List& site_aliases = site_alias_list.GetList();
      for (size_t i = 0; i < site_aliases.size(); ++i) {
        const ValidateSiteResult alias_result =
            ParseSiteAndValidate(site_aliases[i], set_entries);
        if (alias_result.has_error()) {
          if (!IsFatalError(alias_result.error_type())) {
            continue;
          }
          return base::unexpected(
              ParseError(alias_result.error_type(),
                         {kCCTLDsField, site, static_cast<int>(i)}));
        }
        net::SchemefulSite alias = alias_result.site();
        const absl::optional<std::string> alias_site_without_tld =
            RemoveTldFromSite(alias);
        if (!alias_site_without_tld.has_value()) {
          continue;
        }

        if (alias_site_without_tld != site_without_tld) {
          warnings_.push_back(
              ParseWarning(ParseWarningType::kAliasNotCctldVariant,
                           {kCCTLDsField, site, static_cast<int>(i)}));
          continue;
        }
        aliases.emplace_back(std::move(alias), site_as_schemeful_site);
      }
    }

    return aliases;
  }

  // Parses a single base::Value into a net::SchemefulSite, and verifies that it
  // is not already included in this set or any other encountered by this
  // context.
  ValidateSiteResult ParseSiteAndValidate(
      const base::Value& item,
      const std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>&
          set_entries) const {
    if (!item.is_string()) {
      return ValidateSiteResult(ParseErrorType::kInvalidType);
    }

    const ValidateSiteResult result = Canonicalize(item.GetString());

    if (result.has_site()) {
      const net::SchemefulSite& site = result.site();
      if (base::Contains(
              set_entries, site,
              &std::pair<net::SchemefulSite, net::FirstPartySetEntry>::first)) {
        if (result.modified_host()) {
          // If the set repeats this site because the PSL caused us to modify
          // this site's host, we drop this site.
          invalid_keys_.insert(result.site());
          return ValidateSiteResult(result.site(),
                                    ParseErrorType::kInvalidDomain);
        }
        return ValidateSiteResult(ParseErrorType::kRepeatedDomain);
      }

      if (elements_.contains(site)) {
        if (result.modified_host()) {
          // If the sets are nondisjoint because the PSL caused us to modify
          // this site's host, we drop this site.
          invalid_keys_.insert(result.site());
          return ValidateSiteResult(result.site(),
                                    ParseErrorType::kInvalidDomain);
        }
        return ValidateSiteResult(ParseErrorType::kNonDisjointSets);
      }
    }

    return result;
  }

  // Returns true iff the key or value of `pair` corresponds to a domain that
  // is considered invalid. Inserts into `possible_singletons` if it is
  // non-nullptr and `pair` is invalid in a way that might create a singleton
  // set.
  bool IsInvalidEntry(
      const std::pair<net::SchemefulSite, net::FirstPartySetEntry> pair,
      base::flat_set<net::SchemefulSite>* possible_singletons) const {
    const net::SchemefulSite& key = pair.first;
    const net::FirstPartySetEntry& entry = pair.second;
    return base::ranges::any_of(
        invalid_keys_, [&](const net::SchemefulSite& invalid_key) -> bool {
          const bool key_matches = invalid_key == key;
          const bool primary_matches = invalid_key == entry.primary();
          if (key_matches && !primary_matches && possible_singletons) {
            // This is a member whose primary might end up being a
            // singleton, since it's losing at least one member (and it
            // itself isn't invalid).
            possible_singletons->insert(entry.primary());
          }
          return key_matches || primary_matches;
        });
  }

  // Returns true iff the key or value of `pair` is a domain that is considered
  // invalid. Inserts into `possible_singletons` if `pair` is invalid in a way
  // that might create a singleton set.
  bool IsInvalidAlias(
      const std::pair<net::SchemefulSite, net::SchemefulSite> pair,
      base::flat_set<net::SchemefulSite>& possible_singletons,
      const std::vector<SetsMap::value_type>& sets) const {
    const net::SchemefulSite& alias = pair.first;
    const net::SchemefulSite& canonical = pair.second;
    return base::ranges::any_of(
        invalid_keys_, [&](const net::SchemefulSite& invalid_key) -> bool {
          const bool alias_matches = invalid_key == alias;
          const bool canonical_matches = invalid_key == canonical;
          if (alias_matches && !canonical_matches) {
            const bool is_primary = base::Contains(
                sets, canonical,
                [](const std::pair<net::SchemefulSite, net::FirstPartySetEntry>&
                       set_entry) { return set_entry.second.primary(); });
            if (is_primary) {
              // If we're erasing the alias but not the canonical site, and the
              // canonical site is a primary, it might end up becoming a
              // singleton, since it's losing at least one member (and it itself
              // isn't invalid).
              possible_singletons.insert(canonical);
            }
          }
          return alias_matches || canonical_matches;
        });
  }

  // Whether errors should be logged as they're encountered.
  const bool emit_errors_;
  // Whether to ignore subset limits while parsing.
  const bool exempt_from_limits_;
  // The previously encountered set elements. This should be kept up-to-date via
  // `AddSet` while using this context.
  base::flat_set<SetsMap::key_type> elements_;
  // The warnings encountered so far. Mutable so that we can accumulate warnings
  // even in const methods.
  mutable std::vector<ParseWarning> warnings_;
  // The keys (sites) that no set ought to be allowed to include, found while
  // parsing.
  mutable base::flat_set<SetsMap::key_type> invalid_keys_;
};

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
  ValidateSiteResult result =
      ParseContext(emit_errors, /*exempt_from_limits=*/false)
          .Canonicalize(origin_string);
  if (result.has_error()) {
    return absl::nullopt;
  }
  return result.site();
}

SetsAndAliases FirstPartySetParser::ParseSetsFromStream(std::istream& input,
                                                        bool emit_errors,
                                                        bool emit_metrics) {
  std::vector<SetsMap::value_type> sets;
  std::vector<Aliases::value_type> aliases;
  ParseContext context(emit_errors, /*exempt_from_limits=*/false);
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
    base::expected<SetsAndAliases, ParseError> parsed =
        context.ParseSet(*maybe_value);
    if (!parsed.has_value()) {
      if (!IsFatalError(parsed.error().type())) {
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

    context.AddSet(parsed.value());

    base::ranges::move(parsed.value().first, std::back_inserter(sets));
    base::ranges::move(parsed.value().second, std::back_inserter(aliases));
    successfully_parsed_sets++;
  }

  context.PostProcessSets(sets, aliases);

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
  ParseContext context(/*emit_errors=*/false, /*exempt_from_limits=*/true);
  auto set_lists = [&]() -> base::expected<ParsedPolicySetLists,
                                           FirstPartySetsHandler::ParseError> {
    ASSIGN_OR_RETURN(std::vector<SingleSet> replacements,
                     context.GetPolicySetsFromList(
                         policy.FindList(kFirstPartySetPolicyReplacementsField),
                         PolicySetType::kReplacement));
    ASSIGN_OR_RETURN(std::vector<SingleSet> additions,
                     context.GetPolicySetsFromList(
                         policy.FindList(kFirstPartySetPolicyAdditionsField),
                         PolicySetType::kAddition));
    return ParsedPolicySetLists(std::move(replacements), std::move(additions));
  }();

  context.PostProcessSetLists(set_lists);

  return FirstPartySetParser::PolicyParseResult(std::move(set_lists),
                                                context.warnings());
}

std::ostream& operator<<(
    std::ostream& os,
    const FirstPartySetParser::ParsedPolicySetLists& lists) {
  os << "additions: {";
  for (const auto& set : lists.additions) {
    for (const auto& pair : set) {
      os << pair.first << " -> " << pair.second << ", ";
    }
  }
  os << "}, replacements: {";
  for (const auto& set : lists.replacements) {
    for (const auto& pair : set) {
      os << pair.first << " -> " << pair.second << ", ";
    }
  }
  os << "}";
  return os;
}

}  // namespace content
