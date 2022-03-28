// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <cstdint>
#include <memory>
#include <sstream>

#include "net/base/schemeful_site.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string string_input(reinterpret_cast<const char*>(data), size);
  std::istringstream stream(string_input);
  FirstPartySetParser::ParseSetsFromStream(stream);

  // We deserialize -> serialize -> deserialize the input and make sure the
  // outcomes from the two deserialization matches.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> deserialized =
      FirstPartySetParser::DeserializeFirstPartySets(string_input);
  std::string serialized_input =
      FirstPartySetParser::SerializeFirstPartySets(deserialized);
  // The inputs that have hosts contain more than one "." will cause
  // SchemefulSite to consider the registrable domain to start with the last
  // "." during the first deserialization; those hosts that start with '.'
  // are serialized again and then result in empty registrable domain during the
  // second deserialization.
  //
  // We don't run the fuzzer on inputs that are lossy due to URL parsing instead
  // of the FirstPartySetsParser.
  for (const auto& pair : deserialized) {
    if (base::StartsWith(pair.first.GetInternalOriginForTesting().host(),
                         ".") ||
        base::StartsWith(pair.second.GetInternalOriginForTesting().host(),
                         ".")) {
      return 0;
    }
  }
  CHECK(deserialized ==
        FirstPartySetParser::DeserializeFirstPartySets(serialized_input));

  return 0;
}

}  // namespace content
