// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "content/browser/cookie_insight_list/cookie_readiness_list/cookie_readiness_list_parser.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace content {
namespace {
class Environment {
 public:
  Environment() {
    // Sets min log level to FATAL for performance.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    // ICU is as of 2024-12-01 reachable from here so we need to initialize ICU
    // to avoid hitting NOTREACHED()s. See https://crbug.com/381362591.
    CHECK(base::i18n::InitializeICU());
  }
  // Required by ICU integration according to several other fuzzer environments.
  // TODO(pbos): Consider breaking out this fuzzer environment to something
  // shared among most fuzzers. See net/base/fuzzer_test_support.cc and consider
  // moving something like that into the commons.
  base::AtExitManager at_exit_manager;
};

Environment* const environment = new Environment();

void ParseReadinessListSuccessfullyParsesAnyString(const std::string& input) {
  CookieReadinessListParser::ParseReadinessList(input);
}

FUZZ_TEST(CookieReadinessListParserFuzzTest,
          ParseReadinessListSuccessfullyParsesAnyString)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithSeeds(
        {R"({})", R"({[]})", R"({"entries": []})",
         R"({"entries": [
        {
            "domains": [
                "example.com",
            ]
        }]
    })",
         R"({"entries": [
        {
            "tableEntryUrl": "url"
        }]
    })",
         R"({"entries": [
        {
            "domains": [
                "example.com",
            ],
            "tableEntryUrl": "url"
        }]
    })",
         R"({"entries": [
        {
            "domains": [
                ""
            ],
            "invalidField": invalidInput
        }]
    })"}));
}  // namespace
}  // namespace content
