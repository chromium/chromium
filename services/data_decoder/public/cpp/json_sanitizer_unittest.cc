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

// Verifies that |json| is rejected by the sanitizer as an invalid string.
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

  // Valid escaped UTF-16 that encodes non-characters.
  CheckSuccess("[\"\\uFDD0\"]");         // U+FDD0
  CheckSuccess("[\"\\uFDDF\"]");         // U+FDDF
  CheckSuccess("[\"\\uFDEF\"]");         // U+FDEF
  CheckSuccess("[\"\\uFFFE\"]");         // U+FFFE
  CheckSuccess("[\"\\uFFFF\"]");         // U+FFFF
  CheckSuccess("[\"\\uD83F\\uDFFE\"]");  // U+01FFFE
  CheckSuccess("[\"\\uD83F\\uDFFF\"]");  // U+01FFFF
  CheckSuccess("[\"\\uD87F\\uDFFE\"]");  // U+02FFFE
  CheckSuccess("[\"\\uD87F\\uDFFF\"]");  // U+02FFFF
  CheckSuccess("[\"\\uD8BF\\uDFFE\"]");  // U+03FFFE
  CheckSuccess("[\"\\uD8BF\\uDFFF\"]");  // U+03FFFF
  CheckSuccess("[\"\\uD8FF\\uDFFE\"]");  // U+04FFFE
  CheckSuccess("[\"\\uD8FF\\uDFFF\"]");  // U+04FFFF
  CheckSuccess("[\"\\uD93F\\uDFFE\"]");  // U+05FFFE
  CheckSuccess("[\"\\uD93F\\uDFFF\"]");  // U+05FFFF
  CheckSuccess("[\"\\uD97F\\uDFFE\"]");  // U+06FFFE
  CheckSuccess("[\"\\uD97F\\uDFFF\"]");  // U+06FFFF
  CheckSuccess("[\"\\uD9BF\\uDFFE\"]");  // U+07FFFE
  CheckSuccess("[\"\\uD9BF\\uDFFF\"]");  // U+07FFFF
  CheckSuccess("[\"\\uD9FF\\uDFFE\"]");  // U+08FFFE
  CheckSuccess("[\"\\uD9FF\\uDFFF\"]");  // U+08FFFF
  CheckSuccess("[\"\\uDA3F\\uDFFE\"]");  // U+09FFFE
  CheckSuccess("[\"\\uDA3F\\uDFFF\"]");  // U+09FFFF
  CheckSuccess("[\"\\uDA7F\\uDFFE\"]");  // U+0AFFFE
  CheckSuccess("[\"\\uDA7F\\uDFFF\"]");  // U+0AFFFF
  CheckSuccess("[\"\\uDABF\\uDFFE\"]");  // U+0BFFFE
  CheckSuccess("[\"\\uDABF\\uDFFF\"]");  // U+0BFFFF
  CheckSuccess("[\"\\uDAFF\\uDFFE\"]");  // U+0CFFFE
  CheckSuccess("[\"\\uDAFF\\uDFFF\"]");  // U+0CFFFF
  CheckSuccess("[\"\\uDB3F\\uDFFE\"]");  // U+0DFFFE
  CheckSuccess("[\"\\uDB3F\\uDFFF\"]");  // U+0DFFFF
  CheckSuccess("[\"\\uDB7F\\uDFFE\"]");  // U+0EFFFE
  CheckSuccess("[\"\\uDB7F\\uDFFF\"]");  // U+0EFFFF
  CheckSuccess("[\"\\uDBBF\\uDFFE\"]");  // U+0FFFFE
  CheckSuccess("[\"\\uDBBF\\uDFFF\"]");  // U+0FFFFF
  CheckSuccess("[\"\\uDBFF\\uDFFE\"]");  // U+10FFFE
  CheckSuccess("[\"\\uDBFF\\uDFFF\"]");  // U+10FFFF
}

}  // namespace
}  // namespace data_decoder
