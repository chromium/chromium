// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/test/attribution_simulator_input_parser.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    base::i18n::InitializeICU();
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

}  // namespace

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  static Environment env;

  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  absl::optional<base::Value> input = base::JSONReader::Read(
      native_input, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!input)
    return;

  std::ostringstream error_stream;
  ParseAttributionSimulationInput(std::move(*input),
                                  /*offset_time=*/base::Time(), error_stream);
}

}  // namespace content
