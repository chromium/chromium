// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

#include <istream>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Parses newline-delimited First-Party sets (as JSON records) from `input`.
  // Each record should follow the format specified in this
  // document:https://github.com/privacycg/first-party-sets. This function does
  // not check versions or assertions, since it is intended only for sets
  // received by Component Updater.
  //
  // Returns an empty map if parsing or validation of any set failed.
  static base::flat_map<net::SchemefulSite, net::SchemefulSite>
  ParseSetsFromStream(std::istream& input);

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
  static base::flat_map<net::SchemefulSite, net::SchemefulSite>
  DeserializeFirstPartySets(base::StringPiece value);

  // Returns a serialized JSON-encoded string representation of the input. This
  // function does not check or have any special handling for the content of
  // `sets`, e.g. opaque origins are just serialized as "null".
  // The owner -> owner entry is removed from the serialized representation for
  // brevity.
  static std::string SerializeFirstPartySets(
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& sets);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
