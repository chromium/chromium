// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

#include <istream>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "net/first_party_sets/sets_mutation.h"

namespace content {

class CONTENT_EXPORT FirstPartySetParser {
 public:
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
  // Returns an empty GlobalFirstPartySets instance if parsing or validation of
  // any set failed.
  static net::GlobalFirstPartySets ParseSetsFromStream(std::istream& input,
                                                       base::Version version,
                                                       bool emit_errors);

  // Canonicalizes the passed in origin to a registered domain. In particular,
  // this ensures that the origin is non-opaque, is HTTPS, and has a registered
  // domain. Returns std::nullopt in case of any error.
  static std::optional<net::SchemefulSite> CanonicalizeRegisteredDomain(
      std::string_view origin_string,
      bool emit_errors);

  [[nodiscard]] static net::LocalSetDeclaration ParseFromCommandLine(
      const std::string& switch_value);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
