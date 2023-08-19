// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

#include <istream>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT FirstPartySetParser {
 public:
  using SetsMap = base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>;
  // Keys are alias sites, values are their canonical representatives.
  using Aliases = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using SingleSet = SetsMap;
  using SetsAndAliases = std::pair<SetsMap, Aliases>;

  enum class PolicySetType { kReplacement, kAddition };

  struct CONTENT_EXPORT ParsedPolicySetLists {
    ParsedPolicySetLists(std::vector<SingleSet> replacement_list,
                         std::vector<SingleSet> addition_list);

    ParsedPolicySetLists();
    ParsedPolicySetLists(ParsedPolicySetLists&&);
    ParsedPolicySetLists& operator=(ParsedPolicySetLists&&) = default;
    ParsedPolicySetLists(const ParsedPolicySetLists&);
    ParsedPolicySetLists& operator=(const ParsedPolicySetLists&) = default;
    ~ParsedPolicySetLists();

    bool operator==(const ParsedPolicySetLists& other) const;

    std::vector<SingleSet> replacements;
    std::vector<SingleSet> additions;
  };

  using PolicyParseResult = std::pair<
      base::expected<ParsedPolicySetLists, FirstPartySetsHandler::ParseError>,
      std::vector<FirstPartySetsHandler::ParseWarning>>;

  FirstPartySetParser() = delete;
  ~FirstPartySetParser() = delete;

  FirstPartySetParser(const FirstPartySetParser&) = delete;
  FirstPartySetParser& operator=(const FirstPartySetParser&) = delete;

  // Parses newline-delimited First-Party sets (as JSON records) from `input`.
  // Each record should follow the format specified in this
  // document: https://github.com/privacycg/first-party-sets. This function does
  // not check versions or assertions, since it is intended only for sets
  // received by Component Updater.
  //
  // Returns an empty map if parsing or validation of any set failed. Must not
  // be called before field trial state has been initialized.
  static SetsAndAliases ParseSetsFromStream(std::istream& input,
                                            bool emit_errors,
                                            bool emit_metrics);

  // Canonicalizes the passed in origin to a registered domain. In particular,
  // this ensures that the origin is non-opaque, is HTTPS, and has a registered
  // domain. Returns absl::nullopt in case of any error.
  static absl::optional<net::SchemefulSite> CanonicalizeRegisteredDomain(
      const base::StringPiece origin_string,
      bool emit_errors);

  // Parses two lists of First-Party Sets from `policy` using the "replacements"
  // and "additions" list fields if present.
  //
  // Returns the parsed lists and a list of warnings if successful; otherwise,
  // returns an error.
  [[nodiscard]] static PolicyParseResult ParseSetsFromEnterprisePolicy(
      const base::Value::Dict& policy);
};

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const FirstPartySetParser::ParsedPolicySetLists& lists);

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
