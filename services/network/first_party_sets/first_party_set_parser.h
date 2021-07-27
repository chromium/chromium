// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_

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

  // Parses the value in |raw_sets|, which should be the JSON-encoded string
  // representation of a list of set declarations according to the format
  // specified in this document: https://github.com/privacycg/first-party-sets.
  // This function does not check versions or assertions, since it is intended
  // only for sets received by Component Updater.
  //
  // Returns an empty map if parsing or validation of any set failed.
  static base::flat_map<net::SchemefulSite, net::SchemefulSite>
  ParseSetsFromComponentUpdater(base::StringPiece raw_sets);

  // Canonicalizes the passed in origin to a registered domain. In particular,
  // this ensures that the origin is non-opaque, is HTTPS, and has a registered
  // domain. Returns absl::nullopt in case of any error.
  static absl::optional<net::SchemefulSite> CanonicalizeRegisteredDomain(
      const base::StringPiece origin_string,
      bool emit_errors);

  // Generates an in-memory First-Party Sets map from JSON file stored in
  // `path`. This function checks the validity of the domains and the
  // disjointness of the FPSs.
  //
  // Returns an empty map when no previously stored First-Party Sets exists,
  // deserialization fails, or the sets are invalid.
  static base::flat_map<net::SchemefulSite, net::SchemefulSite>
  LoadSetsFromDisk(const base::FilePath& path);

  // Writes First-Party Sets as a serialized JSON-encoded string to location
  // defined by `path`. This function does not check or have any special
  // handling for the content of `sets`, e.g. opaque origins are just serialized
  // as "null".
  //
  // Returns whether or not a map is successfully written.
  static bool MaybeWriteSetsToDisk(
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& sets,
      const base::FilePath& path);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SET_PARSER_H_
