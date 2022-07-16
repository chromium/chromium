// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/luci_test_result.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace perf_test {

namespace {

constexpr char kKeyFilePath[] = "filePath";
constexpr char kKeyContents[] = "contents";
constexpr char kKeyContentType[] = "contentType";
constexpr char kKeyTestResult[] = "testResult";
constexpr char kKeyTestPath[] = "testPath";
constexpr char kKeyVariant[] = "variant";
constexpr char kKeyStatus[] = "status";
constexpr char kKeyExpected[] = "expected";
constexpr char kKeyStartTime[] = "startTime";
constexpr char kKeyRunDuration[] = "runDuration";
constexpr char kKeyOutputArtifacts[] = "outputArtifacts";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyKey[] = "key";
constexpr char kKeyValue[] = "value";

// Returns iso timeformat string of |time| in UTC.
std::string ToUtcIsoTime(base::Time time) {
  base::Time::Exploded utc_exploded;
  time.UTCExplode(&utc_exploded);
  return base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", utc_exploded.year,
      utc_exploded.month, utc_exploded.day_of_month, utc_exploded.hour,
      utc_exploded.minute, utc_exploded.second, utc_exploded.millisecond);
}

std::string ToString(LuciTestResult::Status status) {
  using Status = LuciTestResult::Status;
  switch (status) {
    case Status::kUnspecified:
      return "UNSPECIFIED";
    case Status::kPass:
      return "PASS";
    case Status::kFail:
      return "FAIL";
    case Status::kCrash:
      return "CRASH";
    case Status::kAbort:
      return "ABORT";
    case Status::kSkip:
      return "SKIP";
  }
}

base::Value ToValue(const LuciTestResult::Artifact& artifact) {
  // One and only one of the two optional fields must have value.
  DCHECK(artifact.file_path.has_value() != artifact.contents.has_value());

  base::Value dict(base::Value::Type::DICTIONARY);

  if (artifact.file_path.has_value()) {
    dict.SetStringKey(kKeyFilePath, artifact.file_path->AsUTF8Unsafe());
  } else {
    DCHECK(artifact.contents.has_value());
    dict.SetStringKey(kKeyContents, artifact.contents.value());
  }

  dict.SetStringKey(kKeyContentType, artifact.content_type);
  return dict;
}

base::Value ToValue(const LuciTestResult& result) {
  base::Value test_report(base::Value::Type::DICTIONARY);

  base::Value* test_result = test_report.SetKey(
      kKeyTestResult, base::Value(base::Value::Type::DICTIONARY));
  test_result->SetStringKey(kKeyTestPath, result.test_path());

  if (!result.extra_variant_pairs().empty()) {
    base::Value* variant_dict = test_result->SetKey(
        kKeyVariant, base::Value(base::Value::Type::DICTIONARY));
    for (const auto& pair : result.extra_variant_pairs())
      variant_dict->SetStringKey(pair.first, pair.second);
  }

  test_result->SetStringKey(kKeyStatus, ToString(result.status()));
  test_result->SetBoolKey(kKeyExpected, result.is_expected());

  if (!result.start_time().is_null()) {
    test_result->SetStringKey(kKeyStartTime, ToUtcIsoTime(result.start_time()));
  }
  if (!result.duration().is_zero()) {
    test_result->SetStringKey(
        kKeyRunDuration,
        base::StringPrintf("%.2fs", result.duration().InSecondsF()));
  }

  if (!result.output_artifacts().empty()) {
    base::Value* artifacts_dict = test_result->SetKey(
        kKeyOutputArtifacts, base::Value(base::Value::Type::DICTIONARY));
    for (const auto& pair : result.output_artifacts())
      artifacts_dict->SetKey(pair.first, ToValue(pair.second));
  }

  if (!result.tags().empty()) {
    base::Value* tags_list =
        test_result->SetKey(kKeyTags, base::Value(base::Value::Type::LIST));
    for (const auto& tag : result.tags()) {
      base::Value tag_dict(base::Value::Type::DICTIONARY);
      tag_dict.SetStringKey(kKeyKey, tag.key);
      tag_dict.SetStringKey(kKeyValue, tag.value);
      tags_list->Append(std::move(tag_dict));
    }
  }

  return test_report;
}

std::string ToJson(const LuciTestResult& result) {
  std::string json;
  CHECK(base::JSONWriter::Write(ToValue(result), &json));
  return json;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LuciTestResult::Artifact

LuciTestResult::Artifact::Artifact() = default;
LuciTestResult::Artifact::Artifact(const Artifact& other) = default;
LuciTestResult::Artifact::Artifact(const base::FilePath file_path,
                                   const std::string& content_type)
    : file_path(file_path), content_type(content_type) {}
LuciTestResult::Artifact::Artifact(const std::string& contents,
                                   const std::string& content_type)
    : contents(contents), content_type(content_type) {}
LuciTestResult::Artifact::~Artifact() = default;

///////////////////////////////////////////////////////////////////////////////
// LuciTestResult

LuciTestResult::LuciTestResult() = default;
LuciTestResult::LuciTestResult(const LuciTestResult& other) = default;
LuciTestResult::LuciTestResult(LuciTestResult&& other) = default;
LuciTestResult::~LuciTestResult() = default;

// static
LuciTestResult LuciTestResult::CreateForGTest() {
  LuciTestResult result;

  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();

  std::string test_case_name = test_info->name();
  std::string param_index;

  // If there is a "/", extract |param_index| after it and strip it from
  // |test_case_name|.
  auto pos = test_case_name.rfind('/');
  if (pos != std::string::npos) {
    param_index = test_case_name.substr(pos + 1);
    test_case_name.resize(pos);
  }

  result.set_test_path(base::StringPrintf("%s.%s", test_info->test_suite_name(),
                                          test_case_name.c_str()));

  if (test_info->type_param())
    result.AddVariant("param/instantiation", test_info->type_param());

  if (!param_index.empty())
    result.AddVariant("param/index", param_index);

  result.set_status(test_info->result()->Passed()
                        ? LuciTestResult::Status::kPass
                        : LuciTestResult::Status::kFail);
  // Assumes that the expectation is test passing.
  result.set_is_expected(result.status() == LuciTestResult::Status::kPass);

  // Start timestamp and duration is not set before the test run finishes,
  // e.g. when called from PerformanceTest::TearDownOnMainThread.
  if (test_info->result()->start_timestamp()) {
    result.set_start_time(base::Time::FromTimeT(
        static_cast<time_t>(test_info->result()->start_timestamp() / 1000)));
    result.set_duration(
        base::Milliseconds(test_info->result()->elapsed_time()));
  }

  return result;
}

void LuciTestResult::AddVariant(const std::string& key,
                                const std::string& value) {
  auto result = extra_variant_pairs_.insert({key, value});
  DCHECK(result.second);
}

void LuciTestResult::AddOutputArtifactFile(const std::string& artifact_name,
                                           const base::FilePath& file_path,
                                           const std::string& content_type) {
  Artifact artifact(file_path, content_type);
  auto insert_result = output_artifacts_.insert(
      std::make_pair(artifact_name, std::move(artifact)));
  DCHECK(insert_result.second);
}

void LuciTestResult::AddOutputArtifactContents(
    const std::string& artifact_name,
    const std::string& contents,
    const std::string& content_type) {
  Artifact artifact(contents, content_type);
  auto insert_result = output_artifacts_.insert(
      std::make_pair(artifact_name, std::move(artifact)));
  DCHECK(insert_result.second);
}

void LuciTestResult::AddTag(const std::string& key, const std::string& value) {
  tags_.emplace_back(Tag{key, value});
}

void LuciTestResult::WriteToFile(const base::FilePath& result_file) const {
  const std::string json = ToJson(*this);
  const int json_size = json.size();
  CHECK(WriteFile(result_file, json.data(), json_size) == json_size);
}

}  // namespace perf_test
