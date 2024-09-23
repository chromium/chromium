// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/interop/parser.h"
#include "content/browser/attribution_reporting/interop/runner.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace content {
namespace {

class Environment {
 public:
  Environment()
      : should_dump_input_(std::getenv("LPM_DUMP_NATIVE_INPUT") != nullptr) {
    base::CommandLine::Init(0, nullptr);
    base::i18n::InitializeICU();
    TestTimeouts::Initialize();
  }

  bool should_dump_input() const { return should_dump_input_; }

 private:
  const bool should_dump_input_;
  base::AtExitManager at_exit_manager_;
};

// TODO(crbug.com/332721859) Instead of `json_proto::JsonObject`, create a new
// protobuf that models the test case schema. In addition to improving the
// efficiency of input-space exploration, this could also improve the fuzzer's
// exec/s statistic, since we'd no longer have to serialize and parse the JSON
// object to obtain a `base::Value`.
DEFINE_PROTO_FUZZER(const json_proto::JsonObject& json_object) {
  static Environment env;

  json_proto::JsonProtoConverter converter;
  const std::string serialized_json = converter.Convert(json_object);

  if (env.should_dump_input()) {
    std::cout << "Serialized JSON string: " << serialized_json << std::endl
              << "Hexdump of JSON string: " << base::HexEncode(serialized_json)
              << std::endl;
  }

  std::optional<base::Value> parsed = base::JSONReader::Read(serialized_json);
  // Sometimes, `json_proto::JsonProtoConverter` produces an unparsable string.
  if (!parsed.has_value() || !parsed->is_dict()) {
    return;
  }

  auto run = AttributionInteropRun::Parse(std::move(*parsed).TakeDict(),
                                          AttributionInteropConfig());
  if (!run.has_value()) {
    return;
  }

  static const content::aggregation_service::TestHpkeKey kHpkeKey;

  // TODO(crbug.com/332721859) Fuzz the `AttributionInteropConfig()` parameter
  // when we define a custom protobuf input for this fuzzer.
  std::ignore = RunAttributionInteropSimulation(*std::move(run), kHpkeKey);
}

}  // namespace
}  // namespace content
