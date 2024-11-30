// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_readiness_list/cookie_readiness_list_parser.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace content {
namespace {
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
