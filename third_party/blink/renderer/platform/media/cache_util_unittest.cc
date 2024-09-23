// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/media/cache_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace blink {

// Inputs & expected output for GetReasonsForUncacheability.
struct GRFUTestCase {
  WebURLResponse::HTTPVersion version;
  int status_code;
  const char* headers;
  uint32_t expected_reasons;
};

// Create a new WebURLResponse object.
static WebURLResponse CreateResponse(const GRFUTestCase& test) {
  WebURLResponse response;
  response.SetHttpVersion(test.version);
  response.SetHttpStatusCode(test.status_code);
  for (const std::string& line :
       base::SplitString(test.headers, "\n", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    size_t colon = line.find(": ");
    response.AddHttpHeaderField(WebString::FromUTF8(line.substr(0, colon)),
                                WebString::FromUTF8(line.substr(colon + 2)));
  }
  return response;
}

TEST(CacheUtilTest, GetReasonsForUncacheability) {
  enum { kNoReasons = 0 };

  const GRFUTestCase tests[] = {
      {WebURLResponse::kHTTPVersion_1_1, 206, "ETag: 'fooblort'", kNoReasons},
      {WebURLResponse::kHTTPVersion_1_1, 206, "",
       kNoStrongValidatorOnPartialResponse},
      {WebURLResponse::kHTTPVersion_1_0, 206, "",
       kPre11PartialResponse | kNoStrongValidatorOnPartialResponse},
      {WebURLResponse::kHTTPVersion_1_1, 200, "cache-control: max-Age=42",
       kShortMaxAge},
      {WebURLResponse::kHTTPVersion_1_1, 200, "cache-control: max-Age=4200",
       kNoReasons},
      {WebURLResponse::kHTTPVersion_1_1, 200,
       "Date: Tue, 22 May 2012 23:46:08 GMT\n"
       "Expires: Tue, 22 May 2012 23:56:08 GMT",
       kExpiresTooSoon},
      {WebURLResponse::kHTTPVersion_1_1, 200, "cache-control: must-revalidate",
       kHasMustRevalidate},
      {WebURLResponse::kHTTPVersion_1_1, 200, "cache-control: no-cache",
       kNoCache},
      {WebURLResponse::kHTTPVersion_1_1, 200, "cache-control: no-store",
       kNoStore},
      {WebURLResponse::kHTTPVersion_1_1, 200,
       "cache-control: no-cache\ncache-control: no-store", kNoCache | kNoStore},
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("case: %" PRIuS
                                    ", version: %d, code: %d, headers: %s",
                                    i, tests[i].version, tests[i].status_code,
                                    tests[i].headers));
    EXPECT_EQ(GetReasonsForUncacheability(CreateResponse(tests[i])),
              tests[i].expected_reasons);
  }
}

}  // namespace blink
