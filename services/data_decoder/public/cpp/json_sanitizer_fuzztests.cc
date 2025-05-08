// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace data_decoder {
namespace {

void SanitizerAndBaseJsonReaderDecoderMatch(std::string_view input) {
  [[maybe_unused]] static bool initialized = [] {
#if BUILDFLAG(IS_WIN)
    base::CommandLine::InitUsingArgvForTesting(0, nullptr);
#else
    CHECK(base::CommandLine::Init(0, nullptr));
#endif
    TestTimeouts::Initialize();
    // Note that the documentation says not to call this in tests and to use
    // base::test::ScopedCommandLine instead; however, that doesn't actually
    // work until the current process's command line is first initialized at
    // least once, which it never is in fuzz tests.
    base::CommandLine::Reset();
    return true;
  }();
  std::optional<base::Value> value =
      base::JSONReader::Read(input, base::JSON_PARSE_RFC);
  base::test::TaskEnvironment task_environment;
  base::RunLoop loop;
  JsonSanitizer::Result result;
  JsonSanitizer::Sanitize(std::string(input), base::BindLambdaForTesting(
                                                  [&](JsonSanitizer::Result r) {
                                                    result = r;
                                                    loop.Quit();
                                                  }));
  loop.Run();
  // The JSON parser and the JSON sanitizer should agree on whether or not a
  // given blob of JSON is valid. Additionally, the JSON sanitizer considers
  // something to be invalid JSON if it does not decode to a dict or a list.
  CHECK_EQ(value.has_value() && (value->is_dict() || value->is_list()),
           result.has_value());
}

FUZZ_TEST(JsonSanitizerFuzzTest, SanitizerAndBaseJsonReaderDecoderMatch);

}  // namespace
}  // namespace data_decoder
