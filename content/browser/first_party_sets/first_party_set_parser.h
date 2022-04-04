// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

#include <istream>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
}

namespace content {

class CONTENT_EXPORT FirstPartySetParser {
 public:
  using SetsMap = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using SingleSet =
      std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>;
  using ParseError = FirstPartySetsHandler::ParseError;
  using PolicySetType = FirstPartySetsHandler::PolicySetType;
  using PolicyParsingError = FirstPartySetsHandler::PolicyParsingError;

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

  FirstPartySetParser() = delete;
  ~FirstPartySetParser() = delete;

  FirstPartySetParser(const FirstPartySetParser&) = delete;
  FirstPartySetParser& operator=(const FirstPartySetParser&) = delete;

  // Parses newline-delimited First-Party sets (as JSON records) from `input`.
  // Each record should follow the format specified in this
  // document:https://github.com/privacycg/first-party-sets. This function does
  // not check versions or assertions, since it is intended only for sets
  // received by Component Updater.
  //
  // Returns an empty map if parsing or validation of any set failed.
  static SetsMap ParseSetsFromStream(std::istream& input);

  // Canonicalizes the passed in origin to a registered domain. In particular,
  // this ensures that the origin is non-opaque, is HTTPS, and has a registered
  // domain. Returns absl::nullopt in case of any error.
  static absl::optional<net::SchemefulSite> CanonicalizeRegisteredDomain(
      const base::StringPiece origin_string,
      bool emit_errors);

  // Deserializes a JSON-encoded string obtained from
  // `SerializeFirstPartySets()` into a map. This function checks the validity
  // of the domains and the disjointness of the FPSs.
  //
  // Returns an empty map when deserialization fails, or the sets are invalid.
  static SetsMap DeserializeFirstPartySets(base::StringPiece value);

  // Returns a serialized JSON-encoded string representation of the input. This
  // function does not check or have any special handling for the content of
  // `sets`, e.g. opaque origins are just serialized as "null".
  // The owner -> owner entry is removed from the serialized representation for
  // brevity.
  static std::string SerializeFirstPartySets(const SetsMap& sets);

  // Parses two lists of First-Party Sets from `policy` using the "replacements"
  // and "additions" list fields if present.
  //
  // Returns an optional PolicyParsingError encoding an error and its location
  // if any set in these lists violates the required invariants of a First-Party
  // Set.
  //
  // If no errors are found and `out_sets` is non-null, then `out_sets` will
  // be populated with the parsed replacement sets and parsed addition sets.
  [[nodiscard]] static absl::optional<PolicyParsingError>
  ParseSetsFromEnterprisePolicy(const base::Value::Dict& policy,
                                ParsedPolicySetLists* out_sets);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
