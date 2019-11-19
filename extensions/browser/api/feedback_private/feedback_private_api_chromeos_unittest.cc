// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "extensions/browser/api/feedback_private/feedback_private_api_unittest_base_chromeos.h"
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"

namespace extensions {

namespace {

using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::ReadLogSourceParams;
using base::TimeDelta;

// Converts |params| to a string containing a JSON dictionary within an argument
// list.
std::string ParamsToJSON(const ReadLogSourceParams& params) {
  base::ListValue params_value;
  params_value.Append(params.ToValue());
  std::string params_json_string;
  EXPECT_TRUE(base::JSONWriter::Write(params_value, &params_json_string));

  return params_json_string;
}

}  // namespace

class FeedbackPrivateApiUnittest : public FeedbackPrivateApiUnittestBase {
 public:
  FeedbackPrivateApiUnittest() = default;
  ~FeedbackPrivateApiUnittest() override = default;

  // FeedbackPrivateApiUnittestBase:
  void TearDown() override {
    FeedbackPrivateAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->GetLogSourceAccessManager()
        ->SetTickClockForTesting(nullptr);

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

    std::unique_ptr<base::Value> result_value =
        RunFunctionAndReturnValue(function.get(), ParamsToJSON(params));
    if (!result_value)
      return testing::AssertionFailure() << "No result";

    ReadLogSourceResult result;
    if (!ReadLogSourceResult::Populate(*result_value, &result)) {
      return testing::AssertionFailure()
             << "Unable to parse a valid result from " << *result_value;
    }

    if (result.log_lines.size() != 1) {
      return testing::AssertionFailure()
             << "Expected |log_lines| to contain 1 string, actual number: "
             << result.log_lines.size();
    }

    *result_reader_id = result.reader_id;
    *result_string = result.log_lines[0];

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

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedbackPrivateApiUnittest);
};

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceInvalidId) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;
  params.reader_id = std::make_unique<int>(9999);

  EXPECT_NE("", RunReadLogSourceFunctionWithError(params));
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceNonIncremental) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
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
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;

  int result_reader_id = 0;
  std::string result_string;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_EQ("a", result_string);
  params.reader_id = std::make_unique<int>(result_reader_id);

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

TEST_F(FeedbackPrivateApiUnittest, Anonymize) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;

  int result_reader_id = 0;
  std::string result_string;
  // Skip over all the alphabetic results, to test anonymization of the
  // subsequent MAC address.
  for (int i = 0; i < 26; ++i) {
    EXPECT_TRUE(
        RunReadLogSourceFunction(params, &result_reader_id, &result_string));
    EXPECT_GT(result_reader_id, 0);
    params.reader_id = std::make_unique<int>(result_reader_id);
  }

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ("[MAC OUI=11:22:33 IFACE=1]", result_string);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceMultipleSources) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  int result_reader_id = 0;
  std::string result_string;

  // Attempt to open LOG_SOURCE_MESSAGES twice.
  ReadLogSourceParams params_1st_read;
  params_1st_read.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params_1st_read.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  // Store the reader ID back into the params to set up for the next call.
  params_1st_read.reader_id = std::make_unique<int>(result_reader_id);

  // Create a second reader from the same log source.
  ReadLogSourceParams params_1st_read_repeated;
  params_1st_read_repeated.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params_1st_read_repeated.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read_repeated,
                                       &result_reader_id, &result_string));

  // Attempt to open LOG_SOURCE_UI_LATEST twice.
  ReadLogSourceParams params_2nd_read;
  params_2nd_read.source = api::feedback_private::LOG_SOURCE_UILATEST;
  params_2nd_read.incremental = true;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_NE(*params_1st_read.reader_id, result_reader_id);
  // Store the reader ID back into the params to set up for the next call.
  params_2nd_read.reader_id = std::make_unique<int>(result_reader_id);

  // Create a second reader from the same log source.
  ReadLogSourceParams params_2nd_read_repeated;
  params_2nd_read_repeated.source = api::feedback_private::LOG_SOURCE_UILATEST;
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
  const TimeDelta timeout(TimeDelta::FromMilliseconds(100));
  LogSourceAccessManager::SetMaxNumBurstAccessesForTesting(1);
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  base::SimpleTestTickClock test_clock;
  FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->GetLogSourceAccessManager()
      ->SetTickClockForTesting(&test_clock);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;
  int result_reader_id = 0;
  std::string result_string;

  // |test_clock| must start out at something other than 0, which is interpreted
  // as an invalid value.
  test_clock.Advance(TimeDelta::FromMilliseconds(100));

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(1, result_reader_id);
  params.reader_id = std::make_unique<int>(result_reader_id);

  // Immediately perform another read. This is not allowed. (empty result)
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=120, but it will not be allowed. (empty result)
  test_clock.Advance(TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=150, but still not allowed.
  test_clock.Advance(TimeDelta::FromMilliseconds(30));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=199, but still not allowed. (empty result)
  test_clock.Advance(TimeDelta::FromMilliseconds(49));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=210, annd the access is finally allowed.
  test_clock.Advance(TimeDelta::FromMilliseconds(11));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=309, but it will not be allowed. (empty result)
  test_clock.Advance(TimeDelta::FromMilliseconds(99));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Another read is finally allowed at t=310.
  test_clock.Advance(TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
}

}  // namespace extensions
