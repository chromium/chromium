// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/json_sanitizer.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {
namespace {

// Verifies that |json| can be sanitized by JsonSanitizer, and that the output
// JSON is parsed to the same exact value as the original JSON.
void CheckSuccess(const std::string& json) {
  base::JSONReader::ValueWithError original_parse =
      base::JSONReader::ReadAndReturnValueWithError(json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(original_parse.value);

  base::RunLoop loop;
  bool result_received = false;
  JsonSanitizer::Sanitize(
      json, base::BindLambdaForTesting([&](JsonSanitizer::Result result) {
        result_received = true;
        ASSERT_TRUE(result.value);
        base::JSONReader::ValueWithError reparse =
            base::JSONReader::ReadAndReturnValueWithError(*result.value,
                                                          base::JSON_PARSE_RFC);
        ASSERT_TRUE(reparse.value);
        EXPECT_EQ(*reparse.value, *original_parse.value);
        loop.Quit();
      }));

  // Verify that the API always dispatches its result asynchronously.
  EXPECT_FALSE(result_received);
  loop.Run();
  EXPECT_TRUE(result_received);
}

// Verifies that |json| is rejected by the sanitizer as an invlid string.
void CheckError(const std::string& json) {
  base::RunLoop loop;
  bool result_received = false;
  JsonSanitizer::Sanitize(
      json, base::BindLambdaForTesting([&](JsonSanitizer::Result result) {
        result_received = true;
        EXPECT_FALSE(result.value);
        EXPECT_TRUE(result.error);
        loop.Quit();
      }));

  // Verify that the API always dispatches its result asynchronously.
  EXPECT_FALSE(result_received);
  loop.Run();
  EXPECT_TRUE(result_received);
}

class DataDecoderJsonSanitizerTest : public ::testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(DataDecoderJsonSanitizerTest, Json) {
  // Valid JSON:
  CheckSuccess("{\n  \"foo\": \"bar\"\n}");
  CheckSuccess("[true]");
  CheckSuccess("[42]");
  CheckSuccess("[3.14]");
  CheckSuccess("[4.0]");
  CheckSuccess("[null]");
  CheckSuccess("[\"foo\", \"bar\"]");

  // JSON syntax errors:
  CheckError("");
  CheckError("[");
  CheckError("null");

  // Unterminated array.
  CheckError("[1,2,3,]");
}

TEST_F(DataDecoderJsonSanitizerTest, Nesting) {
  // 10 nested arrays is fine.
  std::string nested(10u, '[');
  nested.append(10u, ']');
  CheckSuccess(nested);

  // 200 nested arrays is too much.
  CheckError(std::string(200u, '[') + std::string(200u, ']'));
}

TEST_F(DataDecoderJsonSanitizerTest, Unicode) {
  // Non-ASCII characters encoded either directly as UTF-8 or escaped as UTF-16:
  CheckSuccess("[\"☃\"]");
  CheckSuccess("[\"\\u2603\"]");
  CheckSuccess("[\"😃\"]");
  CheckSuccess("[\"\\ud83d\\ude03\"]");

  // Malformed UTF-8:
  // A continuation byte outside of a sequence.
  CheckError("[\"\x80\"]");

  // A start byte that is missing a continuation byte.
  CheckError("[\"\xc0\"]");

  // An invalid byte in UTF-8.
  CheckError("[\"\xfe\"]");

  // An overlong encoding (of the letter 'A').
  CheckError("[\"\xc1\x81\"]");

  // U+D83D, a code point reserved for (high) surrogates.
  CheckError("[\"\xed\xa0\xbd\"]");

  // U+4567890, a code point outside of the valid range for Unicode.
  CheckError("[\"\xfc\x84\x95\xa7\xa2\x90\"]");

  // Malformed escaped UTF-16:
  // An unmatched high surrogate.
  CheckError("[\"\\ud83d\"]");

  // An unmatched low surrogate.
  CheckError("[\"\\ude03\"]");

  // A low surrogate followed by a high surrogate.
  CheckError("[\"\\ude03\\ud83d\"]");

  // Valid escaped UTF-16 that encodes non-characters:
  CheckError("[\"\\ufdd0\"]");
  CheckError("[\"\\ufffe\"]");
  CheckError("[\"\\ud83f\\udffe\"]");
}

}  // namespace
}  // namespace data_decoder
