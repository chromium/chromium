// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"

namespace net {
class SchemefulSite;
}

namespace network {

class FirstPartySetParser {
 public:
  FirstPartySetParser() = delete;
  ~FirstPartySetParser() = delete;

  FirstPartySetParser(const FirstPartySetParser&) = delete;
  FirstPartySetParser& operator=(const FirstPartySetParser&) = delete;

  // Parses the value in |raw_sets|, which should be the JSON-encoded string
  // representation of a list of set declarations according to the format
  // specified in this document: https://github.com/privacycg/first-party-sets.
  // This function does not check versions or assertions, since it is intended
  // only for *preloaded* sets.
  //
  // Returns nullptr if parsing or validation of any set failed.
  static std::unique_ptr<base::flat_map<net::SchemefulSite, net::SchemefulSite>>
  ParsePreloadedSets(base::StringPiece raw_sets);

  // Canonicalizes the passed in origin to a registered domain. In particular,
  // this ensures that the origin is non-opaque, is HTTPS, and has a registered
  // domain. Returns base::nullopt in case of any error.
  static base::Optional<net::SchemefulSite> CanonicalizeRegisteredDomain(
      const base::StringPiece origin_string,
      bool emit_errors);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
