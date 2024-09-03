// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

constexpr std::string_view kKeyAggregationServicePayloads =
    "aggregation_service_payloads";

std::string ReadStringFromFile(const base::FilePath& file, bool trim = false) {
  std::string str;
  EXPECT_TRUE(base::ReadFileToString(file, &str));
  if (trim) {
    str = std::string(base::TrimString(str, base::kWhitespaceASCII,
                                       base::TrimPositions::TRIM_ALL));
  }
  return str;
}

base::Value ParseJsonFromFile(const base::FilePath& file) {
  return base::test::ParseJson(ReadStringFromFile(file));
}
}  // namespace

// See
// //content/test/data/private_aggregation/aggregatable_report_goldens/
// README.md.
class PrivateAggregationReportGoldenLatestVersionTest : public testing::Test {
 public:
  void SetUp() override {
    base::PathService::Get(content::DIR_TEST_DATA, &input_dir_);
    input_dir_ = input_dir_.AppendASCII(
        "private_aggregation/aggregatable_report_goldens/latest/");

    ASSERT_OK_AND_ASSIGN(
        PublicKeyset keyset,
        aggregation_service::ReadAndParsePublicKeys(
            input_dir_.AppendASCII("public_key.json"), base::Time::Now()));
    ASSERT_EQ(keyset.keys.size(), 1u);

    aggregation_service().SetPublicKeysForTesting(
        GetAggregationServiceProcessingUrl(
            ::aggregation_service::GetDefaultAggregationCoordinatorOrigin()),
        std::move(keyset));

    std::optional<std::vector<uint8_t>> private_key =
        base::Base64Decode(ReadStringFromFile(
            input_dir_.AppendASCII("private_key.txt"), /*trim=*/true));
    ASSERT_TRUE(private_key);
    ASSERT_EQ(static_cast<int>(private_key->size()), X25519_PRIVATE_KEY_LEN);

    ASSERT_TRUE(EVP_HPKE_KEY_init(full_hpke_key_.get(),
                                  EVP_hpke_x25519_hkdf_sha256(),
                                  private_key->data(), private_key->size()));
  }

 protected:
  void AssembleAndVerifyReport(
      blink::mojom::DebugModeDetailsPtr debug_details,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions,
      PrivateAggregationCallerApi api_identifier,
      std::string_view report_file,
      std::string_view cleartext_payloads_file,
      size_t filtering_id_max_bytes,
      size_t max_num_contributions) {
    const url::Origin kExampleOrigin =
        url::Origin::Create(GURL("https://report.test"));

    base::Value expected_report =
        ParseJsonFromFile(input_dir_.AppendASCII(report_file));
    ASSERT_TRUE(expected_report.is_dict());

    base::Value expected_cleartext_payloads =
        ParseJsonFromFile(input_dir_.AppendASCII(cleartext_payloads_file));
    ASSERT_TRUE(expected_cleartext_payloads.is_list());
    ASSERT_EQ(expected_cleartext_payloads.GetList().size(), 1u);

    const std::string* base64_encoded_expected_cleartext_payload =
        expected_cleartext_payloads.GetList().front().GetIfString();
    ASSERT_TRUE(base64_encoded_expected_cleartext_payload);

    AggregatableReportRequest actual_report =
        PrivateAggregationHost::GenerateReportRequest(
            /*timeout_or_disconnect_timer=*/base::ElapsedTimer(),
            std::move(debug_details),
            /*scheduled_report_time=*/
            base::Time::FromMillisecondsSinceUnixEpoch(1234486400000),
            /*delay_type=*/AggregatableReportRequest::DelayType::Unscheduled,
            /*report_id=*/
            base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"),
            /*reporting_origin=*/kExampleOrigin, api_identifier,
            /*context_id=*/std::nullopt,
            // TODO(alexmt): Generate golden reports for multiple coordinators.
            /*aggregation_coordinator_origin=*/std::nullopt,
            /*specified_filtering_id_max_bytes=*/filtering_id_max_bytes,
            max_num_contributions, std::move(contributions));

    base::RunLoop run_loop;

    aggregation_service().AssembleReport(
        std::move(actual_report),
        base::BindLambdaForTesting(
            [&](AggregatableReportRequest,
                std::optional<AggregatableReport> assembled_report,
                AggregationService::AssemblyStatus status) {
              EXPECT_EQ(status, AggregationService::AssemblyStatus::kOk);
              ASSERT_TRUE(assembled_report);
              EXPECT_TRUE(
                  VerifyReport(assembled_report->GetAsJson(),
                               std::move(expected_report).TakeDict(),
                               *base64_encoded_expected_cleartext_payload))
                  << "There was an error, consider bumping "
                     "api_version, actual output for "
                  << report_file << " is:\n"
                  << assembled_report->GetAsJson();
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  base::MockRepeatingCallback<void(AggregatableReportRequest,
                                   PrivateAggregationBudgetKey)>
      mock_callback_;

 private:
  AggregationServiceImpl& aggregation_service() {
    return *static_cast<AggregationServiceImpl*>(
        static_cast<StoragePartitionImpl*>(
            browser_context_.GetDefaultStoragePartition())
            ->GetAggregationService());
  }

  testing::AssertionResult VerifyReport(
      base::Value::Dict actual_report,
      base::Value::Dict expected_report,
      std::string_view base64_encoded_expected_cleartext_payload) {
    std::optional<base::Value> actual_payloads =
        actual_report.Extract(kKeyAggregationServicePayloads);
    if (!actual_payloads) {
      return testing::AssertionFailure() << kKeyAggregationServicePayloads
                                         << " not present in the actual report";
    }

    std::optional<base::Value> expected_payloads =
        expected_report.Extract(kKeyAggregationServicePayloads);
    if (!expected_payloads) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << " not present in the expected report";
    }

    // All other fields are deterministic.
    if (actual_report != expected_report) {
      return testing::AssertionFailure()
             << "The actual report and expected reports do not match, ignoring "
                "the aggregation service payloads";
    }

    static constexpr std::string_view kKeySharedInfo = "shared_info";
    const std::string* shared_info = expected_report.FindString(kKeySharedInfo);
    if (!shared_info) {
      return testing::AssertionFailure()
             << kKeySharedInfo << " not present in the report";
    }

    if (!actual_payloads->is_list()) {
      return testing::AssertionFailure() << kKeyAggregationServicePayloads
                                         << " not a list in the actual report";
    }

    if (!expected_payloads->is_list()) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << " not a list in the expected report";
    }

    return VerifyAggregationServicePayloads(
        std::move(*actual_payloads).TakeList(),
        std::move(*expected_payloads).TakeList(),
        base64_encoded_expected_cleartext_payload, *shared_info);
  }

  testing::AssertionResult VerifyAggregationServicePayloads(
      base::Value::List actual_payloads,
      base::Value::List expected_payloads,
      std::string_view base64_encoded_expected_cleartext_payload,
      std::string_view shared_info) {
    if (actual_payloads.size() != 1u) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << " not a list of size 1 in the actual report";
    }

    base::Value::Dict* actual_payload = actual_payloads.front().GetIfDict();
    if (!actual_payload) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << "[0] not a dictionary in the actual report";
    }

    if (expected_payloads.size() != 1u) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << " not a list of size 1 in the expected report";
    }

    base::Value::Dict* expected_payload = expected_payloads.front().GetIfDict();
    if (!expected_payload) {
      return testing::AssertionFailure()
             << kKeyAggregationServicePayloads
             << "[0] not a dictionary in the expected report";
    }

    static constexpr std::string_view kKeyPayload = "payload";

    std::optional<base::Value> actual_encrypted_payload =
        actual_payload->Extract(kKeyPayload);
    if (!actual_encrypted_payload) {
      return testing::AssertionFailure()
             << kKeyPayload << " not present in the actual report";
    }

    std::optional<base::Value> expected_encrypted_payload =
        expected_payload->Extract(kKeyPayload);
    if (!expected_encrypted_payload) {
      return testing::AssertionFailure()
             << kKeyPayload << " not present in the expected report";
    }

    // All other fields are deterministic.
    if (*actual_payload != *expected_payload) {
      return testing::AssertionFailure()
             << "The actual and expected aggregation service payloads do not "
                "match, ignoring the encrypted payloads";
    }

    std::vector<uint8_t> actual_decrypted_payload =
        DecryptPayload(actual_encrypted_payload->GetString(), shared_info);
    if (actual_decrypted_payload.empty()) {
      return testing::AssertionFailure()
             << "Failed to decrypt payload in the actual report";
    }

    std::vector<uint8_t> expected_decrypted_payload =
        DecryptPayload(expected_encrypted_payload->GetString(), shared_info);
    if (expected_decrypted_payload.empty()) {
      return testing::AssertionFailure()
             << "Failed to decrypt payload in the expected payload";
    }

    if (actual_decrypted_payload != expected_decrypted_payload) {
      return testing::AssertionFailure()
             << "The actual and expected decrypted payloads do not match";
    }

    if (std::string base64_encoded_decrypted_payload =
            base::Base64Encode(actual_decrypted_payload);
        base64_encoded_decrypted_payload !=
        base64_encoded_expected_cleartext_payload) {
      return testing::AssertionFailure()
             << "The expected cleartext payload does not match actual "
                "decrypted payload, actual output is "
             << base64_encoded_decrypted_payload;
    }

    return testing::AssertionSuccess();
  }

  // Returns empty vector in case of an error.
  std::vector<uint8_t> DecryptPayload(
      std::string_view base64_encoded_encrypted_payload,
      std::string_view shared_info) {
    std::optional<std::vector<uint8_t>> encrypted_payload =
        base::Base64Decode(base64_encoded_encrypted_payload);
    if (!encrypted_payload) {
      return {};
    }

    return aggregation_service::DecryptPayloadWithHpke(
        *encrypted_payload, *full_hpke_key_.get(), shared_info);
  }

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  base::FilePath input_dir_;
  bssl::ScopedEVP_HPKE_KEY full_hpke_key_;
};

namespace {

TEST_F(PrivateAggregationReportGoldenLatestVersionTest, VerifyGoldenReport) {
  struct {
    blink::mojom::DebugModeDetailsPtr debug_details;
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions;
    PrivateAggregationCallerApi api_identifier;
    std::string_view report_file;
    std::string_view cleartext_payloads_file;
    size_t filtering_id_max_bytes = 1;
    bool enable_100_contributions_for_protected_audience = false;
  } kTestCases[] = {
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_1.json",
       .cleartext_payloads_file = "report_1_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_2.json",
       .cleartext_payloads_file = "report_2_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/1, /*value=*/2,
                             /*filtering_id=*/std::nullopt),
                         blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/3, /*value=*/4,
                             /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kSharedStorage,
       .report_file = "report_3.json",
       .cleartext_payloads_file = "report_3_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/1, /*value=*/2,
                             /*filtering_id=*/std::nullopt),
                         blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/3, /*value=*/4,
                             /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kSharedStorage,
       .report_file = "report_4.json",
       .cleartext_payloads_file = "report_4_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/absl::Uint128Max(), /*value=*/2,
           /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_5.json",
       .cleartext_payloads_file = "report_5_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/absl::Uint128Max(), /*value=*/2,
           /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_6.json",
       .cleartext_payloads_file = "report_6_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/0, /*value=*/0, /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kSharedStorage,
       .report_file = "report_7.json",
       .cleartext_payloads_file = "report_7_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_8.json",
       .cleartext_payloads_file = "report_8_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_9.json",
       .cleartext_payloads_file = "report_9_cleartext_payloads.json"},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2,
           /*filtering_id=*/std::numeric_limits<uint64_t>::max())},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_10.json",
       .cleartext_payloads_file = "report_10_cleartext_payloads.json",
       .filtering_id_max_bytes = 8},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_11.json",
       .cleartext_payloads_file = "report_11_cleartext_payloads.json",
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_12.json",
       .cleartext_payloads_file = "report_12_cleartext_payloads.json",
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions =
           std::vector<blink::mojom::AggregatableReportHistogramContribution>(
               99,
               blink::mojom::AggregatableReportHistogramContribution(
                   /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)),
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_13.json",
       .cleartext_payloads_file = "report_13_cleartext_payloads.json",
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions =
           std::vector<blink::mojom::AggregatableReportHistogramContribution>(
               100,
               blink::mojom::AggregatableReportHistogramContribution(
                   /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)),
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_14.json",
       .cleartext_payloads_file = "report_14_cleartext_payloads.json",
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions =
           std::vector<blink::mojom::AggregatableReportHistogramContribution>(
               100, blink::mojom::AggregatableReportHistogramContribution(
                        /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)),
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_15.json",
       .cleartext_payloads_file = "report_15_cleartext_payloads.json",
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions =
           std::vector<blink::mojom::AggregatableReportHistogramContribution>(
               100, blink::mojom::AggregatableReportHistogramContribution(
                        /*bucket=*/1, /*value=*/2,
                        /*filtering_id=*/std::numeric_limits<uint64_t>::max())),
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_16.json",
       .cleartext_payloads_file = "report_16_cleartext_payloads.json",
       .filtering_id_max_bytes = 8,
       .enable_100_contributions_for_protected_audience = true},
      {.debug_details = blink::mojom::DebugModeDetails::New(
           /*is_enabled=*/true,
           /*debug_key=*/blink::mojom::DebugKey::New(/*value=*/123u)),
       .contributions = {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2,
           /*filtering_id=*/std::numeric_limits<uint64_t>::max())},
       .api_identifier = PrivateAggregationCallerApi::kProtectedAudience,
       .report_file = "report_17.json",
       .cleartext_payloads_file = "report_17_cleartext_payloads.json",
       .filtering_id_max_bytes = 8,
       .enable_100_contributions_for_protected_audience = true},
  };

  for (auto& test_case : kTestCases) {
    // This reflects the logic in
    // `PrivateAggregationHost::ContributeToHistogram()` and is copied here to
    // avoid hitting a CHECK().
    bool use_new_report_version =
        base::FeatureList::IsEnabled(
            blink::features::kPrivateAggregationApiFilteringIds) &&
        base::FeatureList::IsEnabled(
            kPrivacySandboxAggregationServiceFilteringIds);
    if (!use_new_report_version) {
      base::ranges::for_each(
          test_case.contributions,
          [](blink::mojom::AggregatableReportHistogramContribution&
                 contribution) { contribution.filtering_id.reset(); });
    }

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (test_case.enable_100_contributions_for_protected_audience) {
      enabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    } else {
      disabled_features.push_back(
          content::kPrivateAggregationApi100ContributionsForProtectedAudience);
    }

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

    AssembleAndVerifyReport(
        std::move(test_case.debug_details), std::move(test_case.contributions),
        std::move(test_case.api_identifier), test_case.report_file,
        test_case.cleartext_payloads_file, test_case.filtering_id_max_bytes,
        PrivateAggregationHost::GetMaxNumContributions(
            test_case.api_identifier));
  }
}

std::vector<base::FilePath> GetLegacyVersions() {
  base::FilePath input_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &input_dir);
  input_dir = input_dir.AppendASCII(
      "content/test/data/private_aggregation/aggregatable_report_goldens");

  std::vector<base::FilePath> input_paths;

  base::FileEnumerator e(input_dir, /*recursive=*/false,
                         base::FileEnumerator::DIRECTORIES);

  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (name.BaseName() == base::FilePath(FILE_PATH_LITERAL("latest"))) {
      continue;
    }

    input_paths.push_back(std::move(name));
  }

  return input_paths;
}

// Verifies that legacy versions are properly labeled/stored. Note that there
// is an implicit requirement that "version" is located in the "shared_info"
// field in the report.
class PrivateAggregationReportGoldenLegacyVersionTest
    : public ::testing::TestWithParam<base::FilePath> {};

// Currently not exercised as there are no legacy versions.
// Will be used when the report version is bumped.
TEST_P(PrivateAggregationReportGoldenLegacyVersionTest, HasExpectedVersion) {
  static constexpr std::string_view prefix = "version_";

  base::FilePath dir = GetParam();

  std::string base_name = dir.BaseName().MaybeAsASCII();
  ASSERT_TRUE(base::StartsWith(base_name, prefix));

  std::string expected_version = base_name.substr(prefix.size());

  base::FileEnumerator e(dir, /*recursive=*/false, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.json"));

  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    base::Value value = ParseJsonFromFile(name);
    if (!value.is_dict()) {
      continue;
    }

    const base::Value::Dict& dict = value.GetDict();
    if (const std::string* shared_info = dict.FindString("shared_info")) {
      base::Value shared_info_value = base::test::ParseJson(*shared_info);
      EXPECT_TRUE(shared_info_value.is_dict()) << name;
      if (!shared_info_value.is_dict()) {
        continue;
      }

      const std::string* version =
          shared_info_value.GetDict().FindString("version");
      EXPECT_TRUE(version) << name;
      if (!version) {
        continue;
      }

      EXPECT_EQ(*version, expected_version) << name;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrivateAggregationReportGoldenLegacyVersionTest,
                         ::testing::ValuesIn(GetLegacyVersions()));

}  // namespace
}  // namespace content
