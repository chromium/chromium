// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "content/public/browser/attribution_reporting.h"
#include "content/public/test/attribution_simulator.h"
#include "content/public/test/attribution_simulator_environment.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  static AttributionSimulatorEnvironment env(/*argc=*/0, /*argv=*/nullptr);

  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  absl::optional<base::Value> input = base::JSONReader::Read(
      native_input, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!input)
    return;

  // TODO(apaseltiner): Fuzz options as well.
  const AttributionSimulationOptions options{
      // Disable noise to make it more likely for fuzz failures to be
      // reproducible.
      .noise_mode = AttributionNoiseMode::kNone,
  };

  std::stringstream error_stream;
  RunAttributionSimulation(std::move(*input), options, error_stream);
}

}  // namespace content
