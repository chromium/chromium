// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <string>

#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();

  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  const String input(native_input.c_str());
  mojom::blink::AttributionSourceData output;
  attribution_response_parsing::ParseSourceRegistrationHeader(input, output);
}

}  // namespace blink
