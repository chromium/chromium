// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <stdlib.h>
#include <iostream>

#include "base/version.h"
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
  FirstPartySetParser::ParseSetsFromStream(stream, base::Version("1.0"), false,
                                           false);
}

}  // namespace content