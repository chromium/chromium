// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/interop/parser.h"
#include "content/browser/attribution_reporting/interop/runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::UnorderedElementsAreArray;

struct AggregatableReportSharedInfo {
  std::string as_string;
  base::Value::Dict as_dict;
};

constexpr char kDefaultConfigFileName[] = "default_config.json";

const aggregation_service::TestHpkeKey kHpkeKey;

base::FilePath GetInputDir() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &input_dir);
  return input_dir.AppendASCII(
      "content/test/data/attribution_reporting/interop");
}

std::vector<base::FilePath> GetInputs() {
  base::FilePath input_dir = GetInputDir();

  std::vector<base::FilePath> input_paths;

  base::FileEnumerator e(input_dir, /*recursive=*/false,
                         base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.json"));

  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (name.BaseName().MaybeAsASCII() == kDefaultConfigFileName) {
      continue;
    }

    input_paths.push_back(std::move(name));
  }

  return input_paths;
}

base::Value::Dict ParseDictFromFile(const base::FilePath& path) {
  std::string json;
  CHECK(base::ReadFileToString(path, &json)) << path;

  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(json,
                                                    base::JSON_ALLOW_COMMENTS);
  CHECK(result.has_value()) << path << ": " << result.error().ToString();

  CHECK(result->is_dict()) << path;
  return std::move(*result).TakeDict();
}

base::Value::List GetDecryptedPayloads(std::optional<base::Value> payloads,
                                       const std::string& shared_info) {
  CHECK(payloads.has_value());

  base::Value::List payloads_list = std::move(*payloads).TakeList();
  CHECK_EQ(payloads_list.size(), 1u);

  base::Value::Dict& payload_dict = payloads_list.front().GetDict();

  std::optional<base::Value> payload = payload_dict.Extract("payload");
  CHECK(payload.has_value());

  std::optional<std::vector<uint8_t>> encrypted_payload =
      base::Base64Decode(payload->GetString());
  CHECK(encrypted_payload.has_value());

  std::vector<uint8_t> decrypted_payload =
      aggregation_service::DecryptPayloadWithHpke(
          *encrypted_payload, kHpkeKey.full_hpke_key(), shared_info);
  std::optional<cbor::Value> deserialized_payload =
      cbor::Reader::Read(decrypted_payload);
  CHECK(deserialized_payload.has_value());
  const cbor::Value::MapValue& payload_map = deserialized_payload->GetMap();
  const auto it = payload_map.find(cbor::Value("data"));
  CHECK(it != payload_map.end());

  base::Value::List list;

  for (const cbor::Value& data : it->second.GetArray()) {
    const cbor::Value::MapValue& data_map = data.GetMap();

    const cbor::Value::BinaryValue& bucket_byte_string =
        data_map.at(cbor::Value("bucket")).GetBytestring();

    absl::uint128 bucket;
    CHECK(
        base::HexStringToUInt128(base::HexEncode(bucket_byte_string), &bucket));

    const cbor::Value::BinaryValue& value_byte_string =
        data_map.at(cbor::Value("value")).GetBytestring();

    uint32_t value;
    CHECK(base::HexStringToUInt(base::HexEncode(value_byte_string), &value));

    // Null reports have null-contribution as well, only ignore the paddings.
    if (!list.empty() && bucket == 0 && value == 0) {
      continue;
    }

    base::Value::Dict dict =
        base::Value::Dict()
            .Set("key", attribution_reporting::HexEncodeAggregationKey(bucket))
            .Set("value", base::checked_cast<int>(value));

    if (data_map.contains(cbor::Value("id"))) {
      const cbor::Value::BinaryValue& id_byte_string =
          data_map.at(cbor::Value("id")).GetBytestring();
      uint64_t id;
      CHECK(base::HexStringToUInt64(base::HexEncode(id_byte_string), &id));
      dict.Set("id", base::NumberToString(id));
    }

    list.Append(std::move(dict));
  }
  return list;
}

void AdjustAggregatableReportBody(base::Value::Dict& report_body) {
  std::optional<base::Value> shared_info = report_body.Extract("shared_info");
  CHECK(shared_info.has_value());
  const std::string& shared_info_str = shared_info->GetString();

  std::optional<base::Value::Dict> shared_info_dict =
      base::JSONReader::ReadDict(shared_info_str, base::JSON_PARSE_RFC);
  CHECK(shared_info_dict.has_value());

  // Report IDs are a source of nondeterminism, so remove them.
  shared_info_dict->Remove("report_id");

  // Set shared_info as a dictionary for easier comparison.
  report_body.Set("shared_info", *std::move(shared_info_dict));

  report_body.Set(
      "histograms",
      GetDecryptedPayloads(report_body.Extract("aggregation_service_payloads"),
                           shared_info_str));
}

class Adjuster : public ReportBodyAdjuster {
 public:
  explicit Adjuster(bool actual) : actual_(actual) {}

  ~Adjuster() override = default;

 private:
  void AdjustEventLevel(base::Value::Dict& report_body) override {
    if (actual_) {
      // Report IDs are a source of nondeterminism, so remove them.
      report_body.Remove("report_id");
    }

    // Ensure that integral values for this field are replaced with the
    // equivalent double, since they are equivalent at the JSON level.
    if (base::Value* rate = report_body.Find("randomized_trigger_rate");
        rate && rate->is_int()) {
      // This coerces the integer to a double.
      *rate = base::Value(rate->GetDouble());
    }
  }

  void AdjustVerboseDebug(std::string_view debug_data_type,
                          base::Value::Dict& body) override {
    ReportBodyAdjuster::AdjustVerboseDebug(debug_data_type, body);

    if (actual_ && debug_data_type == "header-parsing-error") {
      // The header error details are implementation-specific.
      body.Remove("error");
    }
  }

  void AdjustAggregatable(base::Value::Dict& report_body) override {
    if (!actual_) {
      return;
    }

    AdjustAggregatableReportBody(report_body);
  }

  void AdjustAggregatableDebug(base::Value::Dict& report_body) override {
    if (!actual_) {
      return;
    }

    AdjustAggregatableReportBody(report_body);
  }

  const bool actual_;
};

void PreProcessOutput(AttributionInteropOutput& output, const bool actual) {
  Adjuster adjuster(actual);
  for (auto& report : output.reports) {
    MaybeAdjustReportBody(report.url, report.payload, adjuster);
  }
}

class AttributionInteropTest : public ::testing::TestWithParam<base::FilePath> {
 public:
  static void SetUpTestSuite() {
    ASSERT_OK_AND_ASSIGN(
        g_config_, ParseAttributionInteropConfig(ParseDictFromFile(
                       GetInputDir().AppendASCII(kDefaultConfigFileName))));
  }

 protected:
  static AttributionInteropConfig GetConfig() { return g_config_; }

 private:
  static AttributionInteropConfig g_config_;
};

// static
AttributionInteropConfig AttributionInteropTest::g_config_;

// See //content/test/data/attribution_reporting/interop/README.md for the
// JSON schema.
TEST_P(AttributionInteropTest, HasExpectedOutput) {
  base::Value::Dict dict = ParseDictFromFile(GetParam());

  std::optional<base::Value> output = dict.Extract("output");
  ASSERT_TRUE(output && output->is_dict());

  ASSERT_OK_AND_ASSIGN(
      auto expected_output,
      AttributionInteropOutput::Parse(std::move(*output).TakeDict()));

  ASSERT_OK_AND_ASSIGN(
      auto run, AttributionInteropRun::Parse(std::move(dict), GetConfig()));

  ASSERT_OK_AND_ASSIGN(
      AttributionInteropOutput actual_output,
      RunAttributionInteropSimulation(std::move(run), kHpkeKey));

  PreProcessOutput(expected_output, /*actual=*/false);
  PreProcessOutput(actual_output, /*actual=*/true);

  EXPECT_THAT(actual_output,
              Field(&AttributionInteropOutput::reports,
                    UnorderedElementsAreArray(expected_output.reports)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AttributionInteropTest,
    ::testing::ValuesIn(GetInputs()),
    /*name_generator=*/
    [](const ::testing::TestParamInfo<base::FilePath>& info) {
      return info.param.RemoveFinalExtension().BaseName().MaybeAsASCII();
    });

}  // namespace

}  // namespace content
