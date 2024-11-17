// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_constants.h"
#include "extensions/browser/api/feedback_private/feedback_private_api_unittest_base_chromeos.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"
#include "extensions/browser/api/feedback_private/mock_feedback_service.h"

namespace extensions {

namespace {

using api::feedback_private::FeedbackInfo;
using api::feedback_private::ReadLogSourceParams;
using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::SendFeedback::Params;
using feedback::FeedbackData;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::SaveArg;

// Converts |params| to a string containing a JSON dictionary within an argument
// list.
template <typename T>
std::string ParamsToJSON(const T& params) {
  base::Value::List params_value;
  params_value.Append(base::Value(params.ToValue()));
  std::string params_json_string;
  EXPECT_TRUE(base::JSONWriter::Write(params_value, &params_json_string));

  return params_json_string;
}

}  // namespace

class FeedbackPrivateApiUnittest : public FeedbackPrivateApiUnittestBase {
 public:
  FeedbackPrivateApiUnittest() = default;

  FeedbackPrivateApiUnittest(const FeedbackPrivateApiUnittest&) = delete;
  FeedbackPrivateApiUnittest& operator=(const FeedbackPrivateApiUnittest&) =
      delete;

  ~FeedbackPrivateApiUnittest() override = default;

  // FeedbackPrivateApiUnittestBase:
  void TearDown() override {
    FeedbackPrivateAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->GetLogSourceAccessManager()
        ->SetTickClockForTesting(nullptr);

    FeedbackPrivateAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->SetFeedbackServiceForTesting(
            base::MakeRefCounted<FeedbackService>(browser_context()));

    FeedbackPrivateApiUnittestBase::TearDown();
  }

  // Runs the feedbackPrivate.readLogSource() function. See API function
  // definition for argument descriptions.
  //
  // The API function is expected to complete successfully. For running the
  // function with an expectation of an error result, call
  // RunReadLogSourceFunctionWithError().
  //
  // Note that the second argument of the result is a list of strings, but the
  // test class TestSingleLogSource always returns a list containing a single
  // string. To simplify things, the single string result will be returned in
  // |*result_string|, while the reader ID is returned in |*result_reader_id|.
  testing::AssertionResult RunReadLogSourceFunction(
      const ReadLogSourceParams& params,
      int* result_reader_id,
      std::string* result_string) {
    scoped_refptr<FeedbackPrivateReadLogSourceFunction> function =
        base::MakeRefCounted<FeedbackPrivateReadLogSourceFunction>();

    std::optional<base::Value> result_value =
        RunFunctionAndReturnValue(function.get(), ParamsToJSON(params));
    if (!result_value) {
      return testing::AssertionFailure() << "No result";
    }

    auto result = ReadLogSourceResult::FromValue(*result_value);
    if (!result) {
      return testing::AssertionFailure()
             << "Unable to parse a valid result from " << *result_value;
    }

    if (result->log_lines.size() != 1) {
      return testing::AssertionFailure()
             << "Expected |log_lines| to contain 1 string, actual number: "
             << result->log_lines.size();
    }

    *result_reader_id = result->reader_id;
    *result_string = result->log_lines[0];

    return testing::AssertionSuccess();
  }

  // Similar to RunReadLogSourceFunction(), but expects to return an error.
  // Returns a string containing the error message. Does not return any result
  // from the API function.
  std::string RunReadLogSourceFunctionWithError(
      const ReadLogSourceParams& params) {
    scoped_refptr<FeedbackPrivateReadLogSourceFunction> function =
        base::MakeRefCounted<FeedbackPrivateReadLogSourceFunction>();

    return RunFunctionAndReturnError(function.get(), ParamsToJSON(params));
  }

  // Runs the feedbackPrivate.sendFeedback() function. See API function
  // definition for argument descriptions.
  scoped_refptr<FeedbackData> RunSendFeedbackFunction(
      const std::string& args,  // The payload that comes from client
      const FeedbackParams& expected_params) {
    base::Value values = base::test::ParseJson(args);
    EXPECT_TRUE(values.is_list());

    std::optional<api::feedback_private::SendFeedback::Params> params =
        api::feedback_private::SendFeedback::Params::Create(values.GetList());
    EXPECT_TRUE(params);

    scoped_refptr<FeedbackData> actual_feedback_data;
    SetupMockFeedbackService(expected_params, actual_feedback_data);

    auto function = base::MakeRefCounted<FeedbackPrivateSendFeedbackFunction>();

    std::optional<base::Value> result_value =
        RunFunctionAndReturnValue(function.get(), args);
    EXPECT_TRUE(result_value);

    return actual_feedback_data;
  }

  void SetupMockFeedbackService(
      const FeedbackParams& expected_params,
      scoped_refptr<FeedbackData>& actual_feedback_data) {
    auto mock = base::MakeRefCounted<MockFeedbackService>(browser_context());

    // scoped_refptr<FeedbackData> actual_feedback_data;
    EXPECT_CALL(*mock, RedactThenSendFeedback(_, _, _))
        .WillOnce([&](const FeedbackParams& params,
                      scoped_refptr<FeedbackData> feedback_data,
                      SendFeedbackCallback callback) {
          // Pass the feedback data out to verify its properties
          actual_feedback_data = feedback_data;
          // Verify that the flags in params are set correctly
          EXPECT_EQ(expected_params.is_internal_email,
                    params.is_internal_email);
          EXPECT_EQ(expected_params.load_system_info, params.load_system_info);
          EXPECT_EQ(expected_params.send_tab_titles, params.send_tab_titles);
          EXPECT_EQ(expected_params.send_histograms, params.send_histograms);
          EXPECT_EQ(expected_params.send_bluetooth_logs,
                    params.send_bluetooth_logs);
          EXPECT_EQ(expected_params.send_autofill_metadata,
                    params.send_autofill_metadata);

          std::move(callback).Run(true);
        });

    FeedbackPrivateAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->SetFeedbackServiceForTesting(
            static_cast<scoped_refptr<FeedbackService>>(std::move(mock)));
  }
};

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceInvalidId) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LogSource::kMessages;
  params.incremental = true;
  params.reader_id = 9999;

  EXPECT_NE("", RunReadLogSourceFunctionWithError(params));
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceNonIncremental) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LogSource::kMessages;
  params.incremental = false;

  // Test multiple non-incremental reads.
  int result_reader_id = -1;
  std::string result_string;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);

  result_reader_id = -1;
  result_string.clear();
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);

  result_reader_id = -1;
  result_string.clear();
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceIncremental) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LogSource::kMessages;
  params.incremental = true;

  int result_reader_id = 0;
  std::string result_string;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_EQ("a", result_string);
  params.reader_id = result_reader_id;

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ(" bb", result_string);

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ("  ccc", result_string);

  // End the incremental read.
  params.incremental = false;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("   dddd", result_string);

  // The log source will no longer be valid if we try to read it.
  params.incremental = true;
  EXPECT_NE("", RunReadLogSourceFunctionWithError(params));
}

TEST_F(FeedbackPrivateApiUnittest, Redact) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LogSource::kMessages;
  params.incremental = true;

  int result_reader_id = 0;
  std::string result_string;
  // Skip over all the alphabetic results, to test redaction of the subsequent
  // MAC address.
  for (int i = 0; i < 26; ++i) {
    EXPECT_TRUE(
        RunReadLogSourceFunction(params, &result_reader_id, &result_string));
    EXPECT_GT(result_reader_id, 0);
    params.reader_id = result_reader_id;
  }

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ("(MAC OUI=11:22:33 IFACE=1)", result_string);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceMultipleSources) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  int result_reader_id = 0;
  std::string result_string;

  // Attempt to open LOG_SOURCE_MESSAGES twice.
  ReadLogSourceParams params_1st_read;
  params_1st_read.source = api::feedback_private::LogSource::kMessages;
  params_1st_read.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  // Store the reader ID back into the params to set up for the next call.
  params_1st_read.reader_id = result_reader_id;

  // Create a second reader from the same log source.
  ReadLogSourceParams params_1st_read_repeated;
  params_1st_read_repeated.source = api::feedback_private::LogSource::kMessages;
  params_1st_read_repeated.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read_repeated,
                                       &result_reader_id, &result_string));

  // Attempt to open LOG_SOURCE_UI_LATEST twice.
  ReadLogSourceParams params_2nd_read;
  params_2nd_read.source = api::feedback_private::LogSource::kUiLatest;
  params_2nd_read.incremental = true;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_NE(*params_1st_read.reader_id, result_reader_id);
  // Store the reader ID back into the params to set up for the next call.
  params_2nd_read.reader_id = result_reader_id;

  // Create a second reader from the same log source.
  ReadLogSourceParams params_2nd_read_repeated;
  params_2nd_read_repeated.source = api::feedback_private::LogSource::kUiLatest;
  params_2nd_read_repeated.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read_repeated,
                                       &result_reader_id, &result_string));

  // Close the two open log source readers, and make sure new ones can be
  // opened.
  params_1st_read.incremental = false;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read, &result_reader_id,
                                       &result_string));
  EXPECT_EQ(0, result_reader_id);

  params_2nd_read.incremental = false;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read, &result_reader_id,
                                       &result_string));
  EXPECT_EQ(0, result_reader_id);

  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read_repeated,
                                       &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  const int new_read_result_reader_id = result_reader_id;

  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read_repeated,
                                       &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_NE(new_read_result_reader_id, result_reader_id);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceWithAccessTimeouts) {
  const base::TimeDelta timeout(base::Milliseconds(100));
  LogSourceAccessManager::SetMaxNumBurstAccessesForTesting(1);
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  base::SimpleTestTickClock test_clock;
  FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->GetLogSourceAccessManager()
      ->SetTickClockForTesting(&test_clock);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LogSource::kMessages;
  params.incremental = true;
  int result_reader_id = 0;
  std::string result_string;

  // |test_clock| must start out at something other than 0, which is interpreted
  // as an invalid value.
  test_clock.Advance(base::Milliseconds(100));

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(1, result_reader_id);
  params.reader_id = result_reader_id;

  // Immediately perform another read. This is not allowed. (empty result)
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=120, but it will not be allowed. (empty result)
  test_clock.Advance(base::Milliseconds(20));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=150, but still not allowed.
  test_clock.Advance(base::Milliseconds(30));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=199, but still not allowed. (empty result)
  test_clock.Advance(base::Milliseconds(49));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=210, annd the access is finally allowed.
  test_clock.Advance(base::Milliseconds(11));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=309, but it will not be allowed. (empty result)
  test_clock.Advance(base::Milliseconds(99));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Another read is finally allowed at t=310.
  test_clock.Advance(base::Milliseconds(1));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackWithSysInfo) {
  const std::string args = R"([
  {
    "attachedFile": {
      "data": {},
      "name": "C:\\fakepath\\chrome_40px.svg"
    },
    "assistantDebugInfoAllowed": true,
    "attachedFileBlobUuid": "2e3996de-db9e-4c3d-b62c-80d19b6418b9",
    "autofillMetadata": "",
    "categoryTag": "test-tag",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "tester@test.com",
    "flow": "regular",
    "fromAssistant": true,
    "fromAutofill": false,
    "includeBluetoothLogs": true,
    "pageUrl": "https://test.com",
    "productId": 1122,
    "screenshot": {},
    "screenshotBlobUuid": "3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
    "sendAutofillMetadata": false,
    "sendBluetoothLogs": true,
    "sendWifiDebugLogs": false,
    "sendHistograms": true,
    "sendTabTitles": true,
    "systemInformation": [
      {"key": "mem_usage_with_title", "value": "some sensitive info"}
    ],
    "traceId": 9966,
    "useSystemWindowFrame": false
  }
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/true,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/true,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/false};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(9966, feedback_data->trace_id());
  EXPECT_EQ(1122, feedback_data->product_id());
  EXPECT_EQ("chrome_40px.svg", feedback_data->attached_filename());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("test-tag", feedback_data->category_tag());
  EXPECT_EQ("tester@test.com", feedback_data->user_email());
  EXPECT_EQ("2e3996de-db9e-4c3d-b62c-80d19b6418b9",
            feedback_data->attached_file_uuid());
  EXPECT_EQ("3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
            feedback_data->screenshot_uuid());
  EXPECT_EQ("https://test.com", feedback_data->page_url());

  EXPECT_TRUE(feedback_data->from_assistant());
  EXPECT_TRUE(feedback_data->assistant_debug_info_allowed());
  EXPECT_TRUE(feedback_data->sys_info());
  EXPECT_TRUE(feedback_data->sys_info()->count("mem_usage_with_title"));
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackWithoutSysInfo) {
  const std::string args = R"([
  {
    "attachedFile": {
      "data": {},
      "name":""
    },
    "assistantDebugInfoAllowed": false,
    "autofillMetadata": "",
    "categoryTag": "",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "",
    "flow": "regular",
    "fromAssistant": false,
    "fromAutofill": false,
    "includeBluetoothLogs": false,
    "pageUrl": "",
    "screenshot": {},
    "screenshotBlobUuid": "",
    "sendAutofillMetadata": false,
    "sendBluetoothLogs": false,
    "sendWifiDebugLogs": false,
    "sendHistograms": false,
    "sendTabTitles": false,
    "systemInformation": [],
    "useSystemWindowFrame": false
  }
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/false};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(0, feedback_data->trace_id());
  EXPECT_EQ(-1, feedback_data->product_id());
  EXPECT_EQ("", feedback_data->attached_filename());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("", feedback_data->category_tag());
  EXPECT_EQ("", feedback_data->user_email());
  EXPECT_EQ("", feedback_data->attached_file_uuid());
  EXPECT_EQ("", feedback_data->screenshot_uuid());
  EXPECT_EQ("", feedback_data->page_url());

  EXPECT_FALSE(feedback_data->from_assistant());
  EXPECT_FALSE(feedback_data->assistant_debug_info_allowed());
  EXPECT_TRUE(feedback_data->sys_info());
  EXPECT_FALSE(feedback_data->sys_info()->count("mem_usage_with_title"));
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackV2WithOptionsTrue) {
  const std::string args = R"([
  {
    "attachedFile": {
      "data": {},
      "name": "C:\\fakepath\\chrome_40px.svg"
    },
    "assistantDebugInfoAllowed": true,
    "attachedFileBlobUuid": "2e3996de-db9e-4c3d-b62c-80d19b6418b9",
    "autofillMetadata": "",
    "categoryTag": "test-tag",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "tester@test.com",
    "flow": "regular",
    "fromAssistant": true,
    "fromAutofill": false,
    "includeBluetoothLogs": true,
    "pageUrl": "https://test.com",
    "productId": 1122,
    "screenshot": {},
    "screenshotBlobUuid": "3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
    "sendAutofillMetadata": false,
    "sendBluetoothLogs": true,
    "sendWifiDebugLogs": false,
    "sendHistograms": true,
    "sendTabTitles": true,
    "systemInformation": [],
    "traceId": 9966,
    "useSystemWindowFrame": false
  },
  true,
  1.625859975163e+12
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/true,
                                       /*send_tab_titles=*/true,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/true,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/false};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(9966, feedback_data->trace_id());
  EXPECT_EQ(1122, feedback_data->product_id());
  EXPECT_EQ("chrome_40px.svg", feedback_data->attached_filename());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("test-tag", feedback_data->category_tag());
  EXPECT_EQ("tester@test.com", feedback_data->user_email());
  EXPECT_EQ("2e3996de-db9e-4c3d-b62c-80d19b6418b9",
            feedback_data->attached_file_uuid());
  EXPECT_EQ("3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
            feedback_data->screenshot_uuid());
  EXPECT_EQ("https://test.com", feedback_data->page_url());

  EXPECT_TRUE(feedback_data->from_assistant());
  EXPECT_TRUE(feedback_data->assistant_debug_info_allowed());
  EXPECT_TRUE(feedback_data->sys_info());
  EXPECT_TRUE(feedback_data->sys_info()->size() == 0);
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackV2WithOptionsFalse) {
  const std::string args = R"([
  {
    "attachedFile": {
      "data": {},
      "name":""
    },
    "assistantDebugInfoAllowed": false,
    "autofillMetadata": "",
    "categoryTag": "",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "",
    "flow": "regular",
    "fromAssistant": false,
    "fromAutofill": false,
    "includeBluetoothLogs": false,
    "pageUrl": "",
    "screenshot": {},
    "screenshotBlobUuid": "",
    "sendAutofillMetadata": false,
    "sendBluetoothLogs": false,
    "sendWifiDebugLogs": false,
    "sendHistograms": false,
    "sendTabTitles": false,
    "systemInformation": [],
    "useSystemWindowFrame": false
  },
  false,
  1.625859975163e+12
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/false};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(0, feedback_data->trace_id());
  EXPECT_EQ(-1, feedback_data->product_id());
  EXPECT_EQ("", feedback_data->attached_filename());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("", feedback_data->category_tag());
  EXPECT_EQ("", feedback_data->user_email());
  EXPECT_EQ("", feedback_data->attached_file_uuid());
  EXPECT_EQ("", feedback_data->screenshot_uuid());
  EXPECT_EQ("", feedback_data->page_url());

  EXPECT_FALSE(feedback_data->from_assistant());
  EXPECT_FALSE(feedback_data->assistant_debug_info_allowed());
  EXPECT_TRUE(feedback_data->sys_info());
  EXPECT_TRUE(feedback_data->sys_info()->size() == 0);
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackWithAutofillInfo) {
  const std::string args = R"([
  {
    "attachedFile": {
      "data": {},
      "name": "C:\\fakepath\\chrome_40px.svg"
    },
    "assistantDebugInfoAllowed": true,
    "attachedFileBlobUuid": "2e3996de-db9e-4c3d-b62c-80d19b6418b9",
    "autofillMetadata": "test-metadata",
    "categoryTag": "test-tag",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "tester@test.com",
    "flow": "regular",
    "fromAssistant": false,
    "fromAutofill": true,
    "includeBluetoothLogs": true,
    "pageUrl": "https://test.com",
    "productId": 1122,
    "screenshot": {},
    "screenshotBlobUuid": "3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
    "sendAutofillMetadata": true,
    "sendBluetoothLogs": true,
    "sendWifiDebugLogs": false,
    "sendHistograms": true,
    "sendTabTitles": true,
    "systemInformation": [],
    "traceId": 9966,
    "useSystemWindowFrame": false
  }
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/true,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/true,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/true};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(9966, feedback_data->trace_id());
  EXPECT_EQ(1122, feedback_data->product_id());
  EXPECT_EQ("chrome_40px.svg", feedback_data->attached_filename());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("test-tag", feedback_data->category_tag());
  EXPECT_EQ("tester@test.com", feedback_data->user_email());
  EXPECT_EQ("2e3996de-db9e-4c3d-b62c-80d19b6418b9",
            feedback_data->attached_file_uuid());
  EXPECT_EQ("3e72cc3c-550f-49f0-b5d2-bf21f3fbab15",
            feedback_data->screenshot_uuid());
  EXPECT_EQ("https://test.com", feedback_data->page_url());
  EXPECT_EQ("test-metadata", feedback_data->autofill_metadata());

  EXPECT_FALSE(feedback_data->from_assistant());
  EXPECT_TRUE(feedback_data->assistant_debug_info_allowed());
  EXPECT_TRUE(feedback_data->sys_info());
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackAiFlow) {
  const std::string args = R"([
  {
    "aiMetadata": "test-ai-metadata",
    "categoryTag": "test-category",
    "description": "test-desc",
    "descriptionPlaceholder": "",
    "email": "",
    "flow": "ai",
  }
])";

  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false,
                                       /*send_wifi_debug_logs=*/false,
                                       /*send_autofill_metadata=*/false};
  auto feedback_data = RunSendFeedbackFunction(args, expected_params);

  EXPECT_EQ(-1, feedback_data->product_id());
  EXPECT_EQ("test-desc", feedback_data->description());
  EXPECT_EQ("test-category", feedback_data->category_tag());
  EXPECT_EQ("test-ai-metadata", feedback_data->ai_metadata());
}

TEST_F(FeedbackPrivateApiUnittest, SendFeedbackInfoAiFlow) {
  extensions::FeedbackPrivateAPI* api =
      FeedbackPrivateAPI::GetFactoryInstance()->Get(browser_context());

  std::string unused;
  auto feedback_info = api->CreateFeedbackInfo(
      /*description_template=*/unused, /*description_placeholder_text=*/unused,
      /*category_tag=*/unused, /*extra_diagnostics=*/unused,
      /*page_url=*/GURL(),
      /*flow=*/api::feedback_private::FeedbackFlow::kAi,
      /*from_assistant=*/false, /*include_bluetooth_logs=*/false,
      /*show_questionnaire=*/false, /*from_chrome_labs_or_kaleidoscope=*/false,
      /*from_autofill=*/false, /*autofill_metadata=*/base::Value::Dict(),
      /*ai_metadata=*/base::Value::Dict());

  EXPECT_EQ(FeedbackCommon::GetChromeBrowserProductId(),
            feedback_info->product_id);

#if BUILDFLAG(IS_CHROMEOS)
  auto chromeos_ai_metadata = base::Value::Dict();
  chromeos_ai_metadata.Set(feedback::kSeaPenMetadataKey, "true");
  feedback_info = api->CreateFeedbackInfo(
      /*description_template=*/unused, /*description_placeholder_text=*/unused,
      /*category_tag=*/unused, /*extra_diagnostics=*/unused,
      /*page_url=*/GURL(),
      /*flow=*/api::feedback_private::FeedbackFlow::kAi,
      /*from_assistant=*/false, /*include_bluetooth_logs=*/false,
      /*show_questionnaire=*/false, /*from_chrome_labs_or_kaleidoscope=*/false,
      /*from_autofill=*/false, /*autofill_metadata=*/base::Value::Dict(),
      chromeos_ai_metadata);

  EXPECT_EQ(FeedbackCommon::GetChromeOSProductId(), feedback_info->product_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace extensions
