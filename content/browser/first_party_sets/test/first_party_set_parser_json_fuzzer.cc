// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <stdlib.h>
#include <iostream>

#include "net/base/schemeful_site.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace content {

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  std::istringstream stream(native_input);
  FirstPartySetParser::ParseSetsFromStream(stream);

  // We deserialize -> serialize -> deserialize the input and make sure the
  // outcomes from the two deserialization matches.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> deserialized =
      FirstPartySetParser::DeserializeFirstPartySets(native_input);
  std::string serialized_input =
      FirstPartySetParser::SerializeFirstPartySets(deserialized);
  // The inputs that have hosts contain more than one "." will cause
  // SchemefulSite to consider the registrable domain to start with the last
  // "." during the first deserialization; those hosts that start with '.'
  // are then serialized and result in empty registrable domain during the
  // second deserialization.
  //
  // We don't run the fuzzer on inputs that are lossy due to URL parsing instead
  // of the FirstPartySetsParser.
  for (const auto& pair : deserialized) {
    if (base::StartsWith(pair.first.GetInternalOriginForTesting().host(),
                         ".") ||
        base::StartsWith(pair.second.GetInternalOriginForTesting().host(),
                         ".")) {
      return;
    }
  }
  CHECK(deserialized ==
        FirstPartySetParser::DeserializeFirstPartySets(serialized_input));
}

}  // namespace content