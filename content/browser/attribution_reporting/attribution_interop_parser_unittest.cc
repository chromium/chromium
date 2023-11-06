// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <cmath>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

bool operator==(const AttributionConfig::RateLimitConfig& a,
                const AttributionConfig::RateLimitConfig& b) {
  const auto tie = [](const AttributionConfig::RateLimitConfig& config) {
    return std::make_tuple(
        config.time_window, config.max_source_registration_reporting_origins,
        config.max_attribution_reporting_origins, config.max_attributions,
        config.max_reporting_origins_per_source_reporting_site,
        config.origins_per_site_window);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionConfig::EventLevelLimit& a,
                const AttributionConfig::EventLevelLimit& b) {
  const auto tie = [](const AttributionConfig::EventLevelLimit& config) {
    return std::make_tuple(
        config.randomized_response_epsilon, config.max_reports_per_destination,
        config.max_navigation_info_gain, config.max_event_info_gain);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionConfig::AggregateLimit& a,
                const AttributionConfig::AggregateLimit& b) {
  const auto tie = [](const AttributionConfig::AggregateLimit& config) {
    return std::make_tuple(
        config.max_reports_per_destination, config.min_delay, config.delay_span,
        config.null_reports_rate_include_source_registration_time,
        config.null_reports_rate_exclude_source_registration_time,
        config.max_aggregatable_reports_per_source);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionConfig::DestinationRateLimit& a,
                const AttributionConfig::DestinationRateLimit& b) {
  return a.max_total == b.max_total &&
         a.max_per_reporting_site == b.max_per_reporting_site &&
         a.rate_limit_window == b.rate_limit_window;
}

bool operator==(const AttributionConfig& a, const AttributionConfig& b) {
  const auto tie = [](const AttributionConfig& config) {
    return std::make_tuple(config.max_sources_per_origin, config.rate_limit,
                           config.event_level_limit, config.aggregate_limit,
                           config.destination_rate_limit);
  };
  return tie(a) == tie(b);
}

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

using ::attribution_reporting::SuitableOrigin;

// Pick an arbitrary offset time to test correct handling.
constexpr base::Time kOffsetTime = base::Time::UnixEpoch() + base::Days(5);

TEST(AttributionInteropParserTest, EmptyInputParses) {
  const char* const kTestCases[] = {
      R"json({})json",
      R"json({"registrations":[]})json",
  };

  for (const char* json : kTestCases) {
    base::Value::Dict value = base::test::ParseJsonDict(json);
    EXPECT_THAT(ParseAttributionInteropInput(std::move(value), kOffsetTime),
                base::test::ValueIs(IsEmpty()))
        << json;
  }
}

TEST(AttributionInteropParserTest, ValidSourceParses) {
  constexpr char kJson[] = R"json({"registrations": [
    {
      "timestamp": "1643235573123",
      "registration_request": {
        "source_type": "navigation",
        "attribution_src_url": "https://a.r.test",
        "context_origin": "https://a.s.test"
      },
      "responses": [{
        "url": "https://a.r.test",
        "debug_permission": true,
        "response": {
          "Attribution-Reporting-Register-Source": 123
        }
      }]
    },
    {
      "timestamp": "1643235574123",
      "registration_request": {
        "source_type": "event",
        "attribution_src_url": "https://b.r.test",
        "context_origin": "https://b.s.test",
      },
      "responses": [{
        "url": "https://b.r.test",
        "response": {
          "Attribution-Reporting-Register-Source": 456
        }
      }]
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  ASSERT_OK_AND_ASSIGN(
      auto result, ParseAttributionInteropInput(std::move(value), kOffsetTime));
  ASSERT_EQ(result.size(), 2u);

  EXPECT_EQ(result.front().time,
            kOffsetTime + base::Milliseconds(1643235573123));
  EXPECT_EQ(result.front().source_type,
            attribution_reporting::mojom::SourceType::kNavigation);
  EXPECT_EQ(result.front().reporting_origin,
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(result.front().context_origin,
            *SuitableOrigin::Deserialize("https://a.s.test"));
  EXPECT_EQ(result.front().registration, base::Value(123));
  EXPECT_TRUE(result.front().debug_permission);

  EXPECT_EQ(result.back().time,
            kOffsetTime + base::Milliseconds(1643235574123));
  EXPECT_EQ(result.back().source_type,
            attribution_reporting::mojom::SourceType::kEvent);
  EXPECT_EQ(result.back().reporting_origin,
            *SuitableOrigin::Deserialize("https://b.r.test"));
  EXPECT_EQ(result.back().context_origin,
            *SuitableOrigin::Deserialize("https://b.s.test"));
  EXPECT_EQ(result.back().registration, base::Value(456));
  EXPECT_FALSE(result.back().debug_permission);
}

TEST(AttributionInteropParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"registrations": [
    {
      "timestamp": "1643235575123",
      "registration_request": {
        "attribution_src_url": "https://a.r.test",
        "context_origin": " https://b.d.test",
      },
      "responses": [{
        "url": "https://a.r.test",
        "debug_permission": true,
        "response": {
          "Attribution-Reporting-Register-Trigger": 789
        }
      }]
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  ASSERT_OK_AND_ASSIGN(
      auto result, ParseAttributionInteropInput(std::move(value), kOffsetTime));
  ASSERT_EQ(result.size(), 1u);

  EXPECT_EQ(result.front().time,
            kOffsetTime + base::Milliseconds(1643235575123));
  EXPECT_EQ(result.front().reporting_origin,
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(result.front().context_origin,
            *SuitableOrigin::Deserialize("https://b.d.test"));
  EXPECT_EQ(result.front().source_type, absl::nullopt);
  EXPECT_EQ(result.front().registration, base::Value(789));
  EXPECT_TRUE(result.front().debug_permission);
}

struct ParseErrorTestCase {
  const char* expected_failure_substr;
  const char* json;
};

class AttributionInteropParserInputErrorTest
    : public testing::TestWithParam<ParseErrorTestCase> {};

TEST_P(AttributionInteropParserInputErrorTest, InvalidInputFails) {
  const ParseErrorTestCase& test_case = GetParam();

  base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
  auto result = ParseAttributionInteropInput(std::move(value), kOffsetTime);
  EXPECT_THAT(result, base::test::ErrorIs(
                          HasSubstr(test_case.expected_failure_substr)));
}

const ParseErrorTestCase kParseErrorTestCases[] = {
    {
        R"(["registrations"]: must be a list)",
        R"json({"registrations": ""})json",
    },
    {
        R"(["registrations"][0]: must be a dictionary)",
        R"json({"registrations": [""]})json",
    },
    {
        R"(["registrations"][0]["timestamp"]: must be an integer number of)",
        R"json({"registrations": [{}]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]: must be present)",
        R"json({"registrations":[{}]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]: must be a dictionary)",
        R"json({"registrations": [{
          "registration_request": ""
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["attribution_src_url"]: must be a valid, secure origin)",
        R"json({"registrations": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["attribution_src_url"]: must be a valid, secure origin)",
        R"json({"registrations": [{
          "registration_request": {
            "attribution_src_url": "http://r.test"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["context_origin"]: must be a valid, secure origin)",
        R"json({"registrations": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["context_origin"]: must be a valid, secure origin)",
        R"json({"registrations": [{
          "registration_request": {
            "context_origin": "http://s.test"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"]: must be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"]: must be a list)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": ""
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"]: must have size 1)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{}, {}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]: must be a dictionary)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [""]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["url"]: must be a valid, secure origin)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["url"]: must match https://a.r.test)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://b.r.test"
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["response"]: must be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test"
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["response"]: must be a dictionary)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": ""
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["response"]: must contain either source or trigger)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {}
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["response"]: must contain either source or trigger)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Source": {},
              "Attribution-Reporting-Register-Trigger": {}
            }
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["source_type"]: must be either)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "NAVIGATION"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["source_type"]: must be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Source": {}
            }
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["source_type"]: must not be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Trigger": {}
            }
          }]
        }]})json",
    },
    {
        R"(["registrations"][1]["timestamp"]: must be greater than previous time)",
        R"json({"registrations": [
          {
            "timestamp": "1",
            "registration_request": {
              "context_origin": "https://a.d1.test",
              "attribution_src_url": "https://a.r.test"
            },
            "responses": [{
              "url": "https://a.r.test",
              "response": {
                "Attribution-Reporting-Register-Trigger": {}
              }
            }]
          },
          {
            "timestamp": "0",
            "registration_request": {
              "context_origin": "https://a.d1.test",
              "attribution_src_url": "https://a.r.test"
            },
            "responses": [{
              "url": "https://a.r.test",
              "response": {
                "Attribution-Reporting-Register-Trigger": {}
              }
            }]
          },
        ]})json",
    },
};

INSTANTIATE_TEST_SUITE_P(AttributionInteropParserInvalidInputs,
                         AttributionInteropParserInputErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

TEST(AttributionInteropParserTest, ValidConfig) {
  typedef void (*MakeExpectedFunc)(AttributionConfig&);

  const struct {
    const char* json;
    bool required;
    MakeExpectedFunc make_expected;
  } kTestCases[] = {
      {R"json({})json", false, [](AttributionConfig&) {}},
      {R"json({"max_sources_per_origin":"100"})json", false,
       [](AttributionConfig& c) { c.max_sources_per_origin = 100; }},
      {R"json({"max_destinations_per_source_site_reporting_site":"100"})json",
       false,
       [](AttributionConfig& c) {
         c.max_destinations_per_source_site_reporting_site = 100;
       }},
      {R"json({"max_destinations_per_rate_limit_window_reporting_site":"100"})json",
       false,
       [](AttributionConfig& c) {
         c.destination_rate_limit = {.max_per_reporting_site = 100};
       }},
      {R"json({"max_destinations_per_rate_limit_window":"100"})json", false,
       [](AttributionConfig& c) {
         c.destination_rate_limit = {.max_total = 100};
       }},
      {R"json({"destination_rate_limit_window_in_minutes":"5"})json", false,
       [](AttributionConfig& c) {
         c.destination_rate_limit = {.rate_limit_window = base::Minutes(5)};
       }},
      {R"json({"rate_limit_time_window_in_days":"30"})json", false,
       [](AttributionConfig& c) { c.rate_limit.time_window = base::Days(30); }},
      {R"json({"rate_limit_max_source_registration_reporting_origins":"10"})json",
       false,
       [](AttributionConfig& c) {
         c.rate_limit.max_source_registration_reporting_origins = 10;
       }},
      {R"json({"rate_limit_max_attribution_reporting_origins":"10"})json",
       false,
       [](AttributionConfig& c) {
         c.rate_limit.max_attribution_reporting_origins = 10;
       }},
      {R"json({"rate_limit_max_attributions":"10"})json", false,
       [](AttributionConfig& c) { c.rate_limit.max_attributions = 10; }},
      {R"json({"rate_limit_max_reporting_origins_per_source_reporting_site":"2"})json",
       false,
       [](AttributionConfig& c) {
         c.rate_limit.max_reporting_origins_per_source_reporting_site = 2;
       }},
      {R"json({"rate_limit_origins_per_site_window_in_days":"2"})json", false,
       [](AttributionConfig& c) {
         c.rate_limit.origins_per_site_window = base::Days(2);
       }},
      {R"json({"randomized_response_epsilon":"inf"})json", false,
       [](AttributionConfig& c) {
         c.event_level_limit.randomized_response_epsilon =
             std::numeric_limits<double>::infinity();
       }},
      {R"json({"max_event_level_reports_per_destination":"10"})json", false,
       [](AttributionConfig& c) {
         c.event_level_limit.max_reports_per_destination = 10;
       }},
      {R"json({"max_navigation_info_gain":"0.2"})json", false,
       [](AttributionConfig& c) {
         c.event_level_limit.max_navigation_info_gain = 0.2;
       }},
      {R"json({"max_event_info_gain":"0.2"})json", false,
       [](AttributionConfig& c) {
         c.event_level_limit.max_event_info_gain = 0.2;
       }},
      {R"json({"max_aggregatable_reports_per_destination":"10"})json", false,
       [](AttributionConfig& c) {
         c.aggregate_limit.max_reports_per_destination = 10;
       }},
      {R"json({"aggregatable_report_min_delay":"0"})json", false,
       [](AttributionConfig& c) {
         c.aggregate_limit.min_delay = base::TimeDelta();
       }},
      {R"json({"aggregatable_report_delay_span":"0"})json", false,
       [](AttributionConfig& c) {
         c.aggregate_limit.delay_span = base::TimeDelta();
       }},
      {R"json({
        "max_sources_per_origin":"10",
        "max_destinations_per_source_site_reporting_site":"10",
        "max_destinations_per_rate_limit_window_reporting_site": "1",
        "max_destinations_per_rate_limit_window": "2",
        "destination_rate_limit_window_in_minutes": "10",
        "rate_limit_time_window_in_days":"10",
        "rate_limit_max_source_registration_reporting_origins":"20",
        "rate_limit_max_attribution_reporting_origins":"15",
        "rate_limit_max_attributions":"10",
        "rate_limit_max_reporting_origins_per_source_reporting_site":"5",
        "rate_limit_origins_per_site_window_in_days":"5",
        "randomized_response_epsilon":"0.2",
        "max_event_level_reports_per_destination":"10",
        "max_navigation_info_gain":"5.5",
        "max_event_info_gain":"0.5",
        "max_aggregatable_reports_per_destination":"10",
        "aggregatable_report_min_delay":"10",
        "aggregatable_report_delay_span":"20"
      })json",
       true, [](AttributionConfig& c) {
         c.max_sources_per_origin = 10;
         c.max_destinations_per_source_site_reporting_site = 10;

         c.rate_limit.time_window = base::Days(10);
         c.rate_limit.max_source_registration_reporting_origins = 20;
         c.rate_limit.max_attribution_reporting_origins = 15;
         c.rate_limit.max_attributions = 10;
         c.rate_limit.max_reporting_origins_per_source_reporting_site = 5;
         c.rate_limit.origins_per_site_window = base::Days(5);

         c.event_level_limit.randomized_response_epsilon = 0.2;
         c.event_level_limit.max_reports_per_destination = 10;
         c.event_level_limit.max_navigation_info_gain = 5.5;
         c.event_level_limit.max_event_info_gain = 0.5;

         c.aggregate_limit.max_reports_per_destination = 10;
         c.aggregate_limit.min_delay = base::Minutes(10);
         c.aggregate_limit.delay_span = base::Minutes(20);

         c.destination_rate_limit = {.max_total = 2,
                                     .max_per_reporting_site = 1,
                                     .rate_limit_window = base::Minutes(10)};
       }}};

  for (const auto& test_case : kTestCases) {
    AttributionConfig expected;
    test_case.make_expected(expected);

    base::Value::Dict json = base::test::ParseJsonDict(test_case.json);
    if (test_case.required) {
      EXPECT_THAT(ParseAttributionConfig(json), base::test::ValueIs(expected))
          << json;
    } else {
      AttributionConfig config;
      EXPECT_EQ("", MergeAttributionConfig(json, config)) << json;
      EXPECT_EQ(config, expected) << json;
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigPositiveIntegers) {
  const char* const kFields[] = {
      "max_sources_per_origin",
      "max_destinations_per_source_site_reporting_site",
      "max_destinations_per_rate_limit_window_reporting_site",
      "max_destinations_per_rate_limit_window",
      "destination_rate_limit_window_in_minutes",
      "rate_limit_time_window_in_days",
      "rate_limit_max_source_registration_reporting_origins",
      "rate_limit_max_attribution_reporting_origins",
      "rate_limit_max_attributions",
      "rate_limit_max_reporting_origins_per_source_reporting_site",
      "rate_limit_origins_per_site_window_in_days",
      "max_event_level_reports_per_destination",
      "max_aggregatable_reports_per_destination",
  };

  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    for (const char* field : kFields) {
      EXPECT_THAT(result, base::test::ErrorIs(HasSubstr(
                              base::StrCat({"[\"", field,
                                            "\"]: must be a positive integer "
                                            "formatted as base-10 string"}))))
          << field;
    }
  }

  {
    AttributionConfig config;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "0");
    }

    std::string error = MergeAttributionConfig(dict, config);

    for (const char* field : kFields) {
      EXPECT_THAT(
          error,
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a positive integer formatted as base-10 string"})))
          << field;
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigNonNegativeIntegers) {
  const char* const kFields[] = {
      "aggregatable_report_min_delay",
      "aggregatable_report_delay_span",
  };

  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    for (const char* field : kFields) {
      EXPECT_THAT(result, base::test::ErrorIs(HasSubstr(base::StrCat(
                              {"[\"", field,
                               "\"]: must be a non-negative integer "
                               "formatted as base-10 string"}))))
          << field;
    }
  }

  {
    AttributionConfig config;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "-10");
    }

    std::string error = MergeAttributionConfig(dict, config);

    for (const char* field : kFields) {
      EXPECT_THAT(error,
                  HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a non-negative integer "
                                          "formatted as base-10 string"})))
          << field;
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigRandomizedResponseEpsilon) {
  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    EXPECT_THAT(result,
                base::test::ErrorIs(HasSubstr(
                    "[\"randomized_response_epsilon\"]: must be \"inf\" or a "
                    "non-negative double formated as a base-10 string")));
  }
  {
    AttributionConfig config;
    base::Value::Dict dict;
    dict.Set("randomized_response_epsilon", "-1.5");
    std::string error = MergeAttributionConfig(dict, config);
    EXPECT_THAT(
        error,
        HasSubstr("[\"randomized_response_epsilon\"]: must be \"inf\" or a "
                  "non-negative double formated as a base-10 string"));
  }
}

TEST(AttributionInteropParserTest, InvalidConfigMaxInfGain) {
  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(
        result.error(),
        HasSubstr("[\"randomized_response_epsilon\"]: must be \"inf\" or a "
                  "non-negative double formated as a base-10 string"));
  }
  {
    AttributionConfig config;
    base::Value::Dict dict;
    dict.Set("max_navigation_info_gain", "-1.5");
    std::string error = MergeAttributionConfig(dict, config);
    EXPECT_THAT(
        error, HasSubstr("[\"max_navigation_info_gain\"]: must be \"inf\" or a "
                         "non-negative double formated as a base-10 string"));
  }
  {
    AttributionConfig config;
    base::Value::Dict dict;
    dict.Set("max_event_info_gain", "-1.5");
    std::string error = MergeAttributionConfig(dict, config);
    EXPECT_THAT(error,
                HasSubstr("[\"max_event_info_gain\"]: must be \"inf\" or a "
                          "non-negative double formated as a base-10 string"));
  }
}

TEST(AttributionInteropParserTest, ParseOutput) {
  const base::Value kExpectedPayload("abc");

  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<AttributionInteropOutput, std::string>>
        matches;
  } kTestCases[] = {
      {
          "top_level_errors",
          R"json({"foo": []})json",
          ErrorIs(AllOf(
              HasSubstr(R"(["reports"]: must be present)"),
              HasSubstr(R"(["unparsable_registrations"]: must be present)"),
              HasSubstr(R"(["foo"]: unknown field)"))),
      },
      {
          "second_level_errors",
          R"json({
            "reports": [{"foo": null}],
            "unparsable_registrations": [{"bar": 123}]
          })json",
          ErrorIs(AllOf(
              HasSubstr(R"(["reports"][0]["report_time"]: must be an integer)"),
              HasSubstr(R"(["reports"][0]["report_url"]: must be a valid URL)"),
              HasSubstr(R"(["reports"][0]["payload"]: required)"),
              HasSubstr(R"(["reports"][0]["foo"]: unknown field)"),
              HasSubstr(
                  R"(["unparsable_registrations"][0]["time"]: must be an integer)"),
              HasSubstr(
                  R"(["unparsable_registrations"][0]["type"]: must be either)"),
              HasSubstr(
                  R"(["unparsable_registrations"][0]["bar"]: unknown field)"))),
      },
      {
          "unsorted_reports",
          R"json({
            "reports": [
              {
                "report_time": "2",
                "report_url": "https://a.test/x",
                "payload": "abc"
              },
              {
                "report_time": "1",
                "report_url": "https://a.test/y",
                "payload": "def"
              }
             ],
            "unparsable_registrations": []
          })json",
          ErrorIs(HasSubstr(
              R"(["reports"][1]["report_time"]: must be greater than or equal)")),
      },
      {
          "unsorted_unparsable_registrations",
          R"json({
            "unparsable_registrations": [
              {"time": "4", "type": "source"},
              {"time": "3", "type": "trigger"}
             ],
             "reports": []
          })json",
          ErrorIs(HasSubstr(
              R"(["unparsable_registrations"][1]["time"]: must be greater than or equal)")),
      },
      {
          "ok",
          R"json({
            "reports": [{
              "report_time": "123",
              "report_url": "https://a.test/x",
              "payload": "abc"
            }],
            "unparsable_registrations": [{
              "time": "456",
              "type": "trigger"
            }]
          })json",
          ValueIs(AllOf(
              Field(
                  &AttributionInteropOutput::reports,
                  ElementsAre(AllOf(
                      Field(&AttributionInteropOutput::Report::time,
                            base::Time::UnixEpoch() + base::Milliseconds(123)),
                      Field(&AttributionInteropOutput::Report::url,
                            GURL("https://a.test/x")),
                      Field(&AttributionInteropOutput::Report::payload,
                            // `std::ref` needed because `base::Value` isn't
                            // copyable
                            Eq(std::ref(kExpectedPayload)))))),
              Field(
                  &AttributionInteropOutput::unparsable_registrations,
                  ElementsAre(AllOf(
                      Field(&AttributionInteropOutput::UnparsableRegistration::
                                time,
                            base::Time::UnixEpoch() + base::Milliseconds(456)),
                      Field(&AttributionInteropOutput::UnparsableRegistration::
                                type,
                            attribution_reporting::mojom::RegistrationType::
                                kTrigger)))))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
    EXPECT_THAT(AttributionInteropOutput::Parse(std::move(value)),
                test_case.matches);
  }
}

}  // namespace
}  // namespace content
