// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/luci_test_result.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace perf_test {

class LuciTestResultTest : public testing::Test {
 public:
  LuciTestResultTest() = default;
  ~LuciTestResultTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath GetResultFilePath() const {
    return temp_dir_.GetPath().AppendASCII("luci_test_results.json");
  }

  // Validates that |result| is written to file that contains an equivalent JSON
  // as |expected_json|.
  void ValidateResult(const LuciTestResult& result,
                      const std::string& expected_json) {
    const base::FilePath result_file = GetResultFilePath();
    result.WriteToFile(result_file);

    std::string json;
    ASSERT_TRUE(ReadFileToString(GetResultFilePath(), &json));
    base::Optional<base::Value> value = base::JSONReader::Read(json);
    ASSERT_TRUE(value.has_value());

    base::Optional<base::Value> expected_value =
        base::JSONReader::Read(expected_json);
    ASSERT_TRUE(expected_value.has_value());

    EXPECT_EQ(expected_value, value) << "Expected:\n====\n"
                                     << expected_json << "\nActual:\n====\n"
                                     << json;
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(LuciTestResultTest);
};

TEST_F(LuciTestResultTest, Basic) {
  LuciTestResult result;
  result.set_test_path("FakeTestSuite.FakeTest");
  result.set_status(LuciTestResult::Status::kPass);
  result.set_is_expected(true);

  result.AddVariant("variantKey", "variantValue");
  result.AddVariant("param/instantiation", "FooType");
  result.AddVariant("param/index", "0");

  // 2019/9/11 12:30 UTC
  base::Time start_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2019, 9, 3, 11, 12, 30, 0}, &start_time));
  result.set_start_time(start_time);

  result.set_duration(base::TimeDelta::FromMilliseconds(1500));

  result.AddOutputArtifactContents("plain", "plain data", "text/plain");
  result.AddOutputArtifactContents("new_line", "first\nsecond", "text/plain");
  result.AddOutputArtifactFile(
      "file.json", base::FilePath(FILE_PATH_LITERAL("/tmp/file.json")),
      "application/json");
  result.AddTag("tbmv2", "umaMetric");

  const std::string expected_json =
      R"({
          "testResult":{
             "outputArtifacts":{
                "file.json":{
                   "contentType":"application/json",
                   "filePath":"/tmp/file.json"
                },
                "new_line":{
                   "contentType":"text/plain",
                   "contents":"first\nsecond"
                },
                "plain":{
                  "contentType":"text/plain",
                  "contents":"plain data"
                }
             },
             "expected":true,
             "runDuration":"1.50s",
             "startTime":"2019-09-11T12:30:00.000Z",
             "status":"PASS",
             "tags":[
               {"key":"tbmv2","value":"umaMetric"}
             ],
             "variant":{
               "variantKey": "variantValue",
               "param/instantiation": "FooType",
               "param/index": "0"
             },
             "testPath":"FakeTestSuite.FakeTest"
          }
         })";
  ValidateResult(result, expected_json);
}

TEST_F(LuciTestResultTest, Status) {
  using Status = LuciTestResult::Status;

  LuciTestResult result;
  result.set_test_path("FakeTestSuite.Status");

  const std::string json_template =
      R"({
           "testResult":{
             "expected":false,
             "status":"%s",
             "testPath":"FakeTestSuite.Status"
           }
         })";

  const struct {
    Status status;
    const char* status_text;
  } kTestCases[] = {
      {Status::kUnspecified, "UNSPECIFIED"},
      {Status::kPass, "PASS"},
      {Status::kFail, "FAIL"},
      {Status::kCrash, "CRASH"},
      {Status::kAbort, "ABORT"},
      {Status::kSkip, "SKIP"},
  };

  for (const auto& test_case : kTestCases) {
    result.set_status(test_case.status);
    const std::string expected_json =
        base::StringPrintf(json_template.c_str(), test_case.status_text);
    ValidateResult(result, expected_json);
  }
}

///////////////////////////////////////////////////////////////////////////////

class LuciTestResultParameterizedTest
    : public LuciTestResultTest,
      public testing::WithParamInterface<int> {
 public:
  LuciTestResultParameterizedTest() = default;
  ~LuciTestResultParameterizedTest() override = default;
};

TEST_P(LuciTestResultParameterizedTest, Variant) {
  LuciTestResult result = LuciTestResult::CreateForGTest();

  const std::string json_template =
      R"({
           "testResult":{
             "expected":true,
             "status":"PASS",
             "testPath":
                 "ZeroToFiveSequence/LuciTestResultParameterizedTest.Variant",
             "variant":{"param/index":"%d"}
           }
         })";
  const std::string expected_json =
      base::StringPrintf(json_template.c_str(), GetParam());
  ValidateResult(result, expected_json);
}
INSTANTIATE_TEST_SUITE_P(ZeroToFiveSequence,
                         LuciTestResultParameterizedTest,
                         testing::Range(0, 5));

///////////////////////////////////////////////////////////////////////////////

template <typename T>
class LuciTestResultTypedTest : public LuciTestResultTest {
 public:
  LuciTestResultTypedTest() = default;
  ~LuciTestResultTypedTest() override = default;
};

TYPED_TEST_SUITE_P(LuciTestResultTypedTest);

TYPED_TEST_P(LuciTestResultTypedTest, Variant) {
  LuciTestResult result = LuciTestResult::CreateForGTest();

  std::string test_suite_name =
      testing::UnitTest::GetInstance()->current_test_info()->test_suite_name();
  auto pos = test_suite_name.rfind('/');
  ASSERT_NE(pos, std::string::npos);
  std::string type_param_name = test_suite_name.substr(pos + 1);

  const std::string json_template =
      R"({
           "testResult":{
             "expected":true,
             "status":"PASS",
             "testPath":"LuciTestResultTypedTest/%s.Variant",
             "variant":{"param/instantiation":"%s"}
           }
         })";
  // Note that chromium has RTTI disabled. As a result, type_param() and
  // GetTypeName<> always returns a generic "<type>".
  const std::string expected_json =
      base::StringPrintf(json_template.c_str(), type_param_name.c_str(),
                         testing::internal::GetTypeName<TypeParam>().c_str());
  this->ValidateResult(result, expected_json);
}

REGISTER_TYPED_TEST_SUITE_P(LuciTestResultTypedTest, Variant);

using SomeTypes = testing::Types<int, double>;
INSTANTIATE_TYPED_TEST_SUITE_P(, LuciTestResultTypedTest, SomeTypes);

}  // namespace perf_test
