// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/interop/parser.h"

#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using ::attribution_reporting::SuitableOrigin;

TEST(AttributionInteropParserTest, EmptyInputParses) {
  const char* const kTestCases[] = {
      R"json({})json",
      R"json({"registrations":[]})json",
  };

  for (const char* json : kTestCases) {
    base::Value::Dict value = base::test::ParseJsonDict(json);
    EXPECT_THAT(ParseAttributionInteropInput(std::move(value)),
                base::test::ValueIs(IsEmpty()))
        << json;
  }
}

MATCHER_P(SimulationEventTimeIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.time, result_listener);
}

MATCHER_P(StartRequestIs, matcher, "") {
  return ExplainMatchResult(
      VariantWith<AttributionSimulationEvent::StartRequest>(matcher), arg.data,
      result_listener);
}

MATCHER_P(ResponseIs, matcher, "") {
  return ExplainMatchResult(
      VariantWith<AttributionSimulationEvent::Response>(matcher), arg.data,
      result_listener);
}

MATCHER_P(EndRequestIs, matcher, "") {
  return ExplainMatchResult(
      VariantWith<AttributionSimulationEvent::EndRequest>(matcher), arg.data,
      result_listener);
}

MATCHER_P(RequestIdIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.request_id, result_listener);
}

TEST(AttributionInteropParserTest, ValidRegistrationsParse) {
  using Response = ::content::AttributionSimulationEvent::Response;
  using StartRequest = ::content::AttributionSimulationEvent::StartRequest;

  constexpr char kJson[] = R"json({"registrations": [
    {
      "timestamp": "100",
      "registration_request": {
        "Attribution-Reporting-Eligible": "navigation-source",
        "context_origin": "https://a.s.test"
      },
      "responses": [{
        "url": "https://a.r.test/x",
        "debug_permission": true,
        "response": {
          "Attribution-Reporting-Register-Source": {"a": "b"}
        }
      }]
    },
    {
      "timestamp": "101",
      "registration_request": {
        "Attribution-Reporting-Eligible": "event-source",
        "context_origin": "https://b.s.test",
        "fenced": true
      },
      "responses": [
        {
          "url": "https://b.r.test/y",
          "randomized_response": [{
            "trigger_data": 5,
            "report_window_index": 1
          }],
          "response": {
            "Attribution-Reporting-Register-Source": "!!!"
          }
        },
        {
          "url": "https://c.r.test/z",
          "timestamp": "102",
          "null_aggregatable_reports_days": [1, 5],
          "response": {
            "Attribution-Reporting-Register-Trigger": "***"
          }
        }
      ]
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  ASSERT_OK_AND_ASSIGN(auto result,
                       ParseAttributionInteropInput(std::move(value)));

  const base::Time kExpectedTime1 =
      base::Time::UnixEpoch() + base::Milliseconds(100);
  const base::Time kExpectedTime2 =
      base::Time::UnixEpoch() + base::Milliseconds(101);
  const base::Time kExpectedTime3 =
      base::Time::UnixEpoch() + base::Milliseconds(102);

  const int64_t kExpectedRequestId1 = 0;
  const int64_t kExpectedRequestId2 = 1;

  EXPECT_THAT(
      result,
      ElementsAre(
          AllOf(SimulationEventTimeIs(kExpectedTime1),
                StartRequestIs(AllOf(
                    RequestIdIs(kExpectedRequestId1),
                    Field(&StartRequest::context_origin,
                          *SuitableOrigin::Deserialize("https://a.s.test")),
                    Field(&StartRequest::fenced, false),
                    Field(&StartRequest::eligibility,
                          network::mojom::AttributionReportingEligibility::
                              kNavigationSource)))),
          AllOf(SimulationEventTimeIs(kExpectedTime1),
                ResponseIs(AllOf(
                    RequestIdIs(kExpectedRequestId1),
                    Field(&Response::url, GURL("https://a.r.test/x")),
                    Field(&Response::response_headers,
                          ::testing::ResultOf(
                              [](const auto& headers) {
                                return headers->HasHeaderValue(
                                    "Attribution-Reporting-Register-Source",
                                    R"({"a":"b"})");
                              },
                              true)),
                    Field(&Response::randomized_response, std::nullopt),
                    Field(&Response::debug_permission, true)))),
          AllOf(SimulationEventTimeIs(kExpectedTime1),
                EndRequestIs(RequestIdIs(kExpectedRequestId1))),
          AllOf(SimulationEventTimeIs(kExpectedTime2),
                StartRequestIs(AllOf(RequestIdIs(kExpectedRequestId2),
                                     Field(&StartRequest::fenced, true)))),
          AllOf(SimulationEventTimeIs(kExpectedTime2),
                ResponseIs(
                    AllOf(RequestIdIs(kExpectedRequestId2),
                          Field(&Response::randomized_response,
                                Optional(ElementsAre(
                                    attribution_reporting::FakeEventLevelReport{
                                        .trigger_data = 5,
                                        .window_index = 1,
                                    }))),
                          Field(&Response::debug_permission, false)))),
          AllOf(SimulationEventTimeIs(kExpectedTime3),
                ResponseIs(
                    AllOf(Field(&Response::url, GURL("https://c.r.test/z")),
                          Field(&Response::null_aggregatable_reports_days,
                                UnorderedElementsAre(1, 5))))),
          AllOf(SimulationEventTimeIs(kExpectedTime3),
                EndRequestIs(RequestIdIs(kExpectedRequestId2)))));
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
  auto result = ParseAttributionInteropInput(std::move(value));
  EXPECT_THAT(result, ErrorIs(HasSubstr(test_case.expected_failure_substr)));
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
        R"(["registrations"][0]["registration_request"]["fenced"]: must be a bool)",
        R"json({"registrations": [{
          "registration_request": {
            "fenced": 0
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"]: must be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"]: must be a list)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": ""
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]: must be a dictionary)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [""]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][1]["timestamp"]: must be an integer number of milliseconds)",
        R"json({"registrations": [{
          "timestamp": "1",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{}, {}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][1]["timestamp"]: must be greater than previous time)",
        R"json({"registrations": [{
          "timestamp": "1",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [
            {"url": "https://b.test", "response": {}, "timestamp": "2"},
            {"url": "https://c.test", "response": {}, "timestamp": "2"}
          ]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["randomized_response"]: must be a list)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"randomized_response": 1}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["randomized_response"][0]: must be a dictionary)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"randomized_response": [1]}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["randomized_response"][0]["trigger_data"]: must be a uint32)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"randomized_response": [{"trigger_data": "1"}]}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["randomized_response"][0]["report_window_index"]: must be a non-negative integer)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"randomized_response": [{"report_window_index": -1}]}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["url"]: must be a valid URL)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["null_aggregatable_reports_days"]: must be a list)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"null_aggregatable_reports_days": 1}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["null_aggregatable_reports_days"][0]: must be a non-negative integer)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "context_origin": "https://a.s.test"
          },
          "responses": [{"null_aggregatable_reports_days": [-1]}]
        }]})json",
    },
    {
        R"(["registrations"][0]["responses"][0]["response"]: must be present)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
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
            "context_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": ""
          }]
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["Attribution-Reporting-Eligible"]: must be a structured dictionary)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "Attribution-Reporting-Eligible": "!"
          }
        }]})json",
    },
    {
        R"(["registrations"][0]["registration_request"]["Attribution-Reporting-Eligible"]: navigation-source is mutually exclusive)",
        R"json({"registrations": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "Attribution-Reporting-Eligible": "navigation-source, event-source"
          }
        }]})json",
    },
    {
        R"(["registrations"][1]["timestamp"]: must be greater than previous time)",
        R"json({"registrations": [
          {
            "timestamp": "1",
            "registration_request": {
              "context_origin": "https://a.d1.test",
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

INSTANTIATE_TEST_SUITE_P(,
                         AttributionInteropParserInputErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

TEST(AttributionInteropParserTest, ValidConfig) {
  typedef void (*MakeAttributionConfigFunc)(AttributionConfig&);
  typedef void (*MakeInteropConfigFunc)(AttributionInteropConfig&);

  using MakeExpectedFunc =
      absl::variant<MakeAttributionConfigFunc, MakeInteropConfigFunc>;

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
      {R"json({"max_destinations_per_reporting_site_per_day":"20"})json", false,
       [](AttributionConfig& c) {
         c.destination_rate_limit = {.max_per_reporting_site_per_day = 20};
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
      {R"json({"max_settable_event_level_epsilon":"inf"})json", false,
       [](AttributionInteropConfig& c) {
         c.max_event_level_epsilon = std::numeric_limits<double>::infinity();
       }},
      {R"json({"max_event_level_reports_per_destination":"10"})json", false,
       [](AttributionConfig& c) {
         c.event_level_limit.max_reports_per_destination = 10;
       }},
      {R"json({"max_event_level_channel_capacity_navigation":"0.2"})json",
       false,
       [](AttributionConfig& c) {
         c.privacy_math_config.max_channel_capacity_navigation = 0.2;
       }},
      {R"json({"max_event_level_channel_capacity_event":"0.2"})json", false,
       [](AttributionConfig& c) {
         c.privacy_math_config.max_channel_capacity_event = 0.2;
       }},
      {R"json({"max_event_level_channel_capacity_scopes_navigation":"0.2"})json",
       false,
       [](AttributionConfig& c) {
         c.privacy_math_config.max_channel_capacity_scopes_navigation = 0.2;
       }},
      {R"json({"max_event_level_channel_capacity_scopes_event":"0.2"})json",
       false,
       [](AttributionConfig& c) {
         c.privacy_math_config.max_channel_capacity_scopes_event = 0.2;
       }},
      {R"json({"max_trigger_state_cardinality":"4294967295"})json", false,
       [](AttributionInteropConfig& c) {
         c.max_trigger_state_cardinality = 4294967295;
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
      {R"json({"max_aggregatable_debug_budget_per_context_site":"65537"})json",
       false,
       [](AttributionConfig& c) {
         c.aggregatable_debug_rate_limit.max_budget_per_context_site = 65537;
       }},
      {R"json({"max_aggregatable_debug_reports_per_source":"3"})json", false,
       [](AttributionConfig& c) {
         c.aggregatable_debug_rate_limit.max_reports_per_source = 3;
       }},
      {R"json({
        "max_sources_per_origin":"10",
        "max_destinations_per_source_site_reporting_site":"10",
        "max_destinations_per_rate_limit_window_reporting_site": "1",
        "max_destinations_per_rate_limit_window": "2",
        "destination_rate_limit_window_in_minutes": "10",
        "max_destinations_per_reporting_site_per_day": "15",
        "rate_limit_time_window_in_days":"10",
        "rate_limit_max_source_registration_reporting_origins":"20",
        "rate_limit_max_attribution_reporting_origins":"15",
        "rate_limit_max_attributions":"10",
        "rate_limit_max_reporting_origins_per_source_reporting_site":"5",
        "rate_limit_origins_per_site_window_in_days":"5",
        "max_settable_event_level_epsilon":"0.2",
        "max_event_level_reports_per_destination":"10",
        "max_event_level_channel_capacity_navigation":"5.5",
        "max_event_level_channel_capacity_event":"0.5",
        "max_event_level_channel_capacity_scopes_navigation":"5.55",
        "max_event_level_channel_capacity_scopes_event":"0.55",
        "max_trigger_state_cardinality":"10",
        "max_aggregatable_reports_per_destination":"10",
        "aggregatable_report_min_delay":"10",
        "aggregatable_report_delay_span":"20",
        "aggregation_coordinator_origins":["https://c.test/123"],
        "max_aggregatable_debug_budget_per_context_site": "1024",
        "max_aggregatable_debug_reports_per_source": "10"
      })json",
       true, [](AttributionInteropConfig& config) {
         AttributionConfig& c = config.attribution_config;

         c.max_sources_per_origin = 10;
         c.max_destinations_per_source_site_reporting_site = 10;

         c.rate_limit.time_window = base::Days(10);
         c.rate_limit.max_source_registration_reporting_origins = 20;
         c.rate_limit.max_attribution_reporting_origins = 15;
         c.rate_limit.max_attributions = 10;
         c.rate_limit.max_reporting_origins_per_source_reporting_site = 5;
         c.rate_limit.origins_per_site_window = base::Days(5);

         config.max_event_level_epsilon = 0.2;
         c.event_level_limit.max_reports_per_destination = 10;
         c.privacy_math_config.max_channel_capacity_navigation = 5.5;
         c.privacy_math_config.max_channel_capacity_event = 0.5;
         c.privacy_math_config.max_channel_capacity_scopes_navigation = 5.55;
         c.privacy_math_config.max_channel_capacity_scopes_event = 0.55;
         config.max_trigger_state_cardinality = 10;

         c.aggregate_limit.max_reports_per_destination = 10;
         c.aggregate_limit.min_delay = base::Minutes(10);
         c.aggregate_limit.delay_span = base::Minutes(20);

         c.destination_rate_limit = {
             .max_total = 2,
             .max_per_reporting_site = 1,
             .rate_limit_window = base::Minutes(10),
             .max_per_reporting_site_per_day = 15,
         };

         config.aggregation_coordinator_origins.emplace_back(
             url::Origin::Create(GURL("https://c.test")));

         c.aggregatable_debug_rate_limit.max_budget_per_context_site = 1024;
         c.aggregatable_debug_rate_limit.max_reports_per_source = 10;
       }}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.json);

    AttributionInteropConfig expected;
    absl::visit(base::Overloaded{
                    [&](MakeAttributionConfigFunc f) {
                      f(expected.attribution_config);
                    },
                    [&](MakeInteropConfigFunc f) { f(expected); },
                },
                test_case.make_expected);

    base::Value::Dict dict = base::test::ParseJsonDict(test_case.json);
    if (test_case.required) {
      EXPECT_THAT(ParseAttributionInteropConfig(std::move(dict)),
                  ValueIs(expected));
    } else {
      AttributionInteropConfig config;
      EXPECT_THAT(MergeAttributionInteropConfig(std::move(dict), config),
                  base::test::HasValue());
      EXPECT_EQ(config, expected);
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
      "max_aggregatable_debug_budget_per_context_site",
      "max_aggregatable_debug_reports_per_source"};

  {
    auto result = ParseAttributionInteropConfig(base::Value::Dict());
    for (const char* field : kFields) {
      EXPECT_THAT(
          result,
          ErrorIs(HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a positive integer "
                                          "formatted as base-10 string"}))));
    }
  }

  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "0");
    }

    auto result = MergeAttributionInteropConfig(std::move(dict), config);

    for (const char* field : kFields) {
      EXPECT_THAT(
          result,
          ErrorIs(HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a positive integer "
                                          "formatted as base-10 string"}))));
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigNonNegativeIntegers) {
  const char* const kFields[] = {
      "aggregatable_report_min_delay",
      "aggregatable_report_delay_span",
  };

  {
    auto result = ParseAttributionInteropConfig(base::Value::Dict());
    for (const char* field : kFields) {
      EXPECT_THAT(
          result,
          ErrorIs(HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a non-negative integer "
                                          "formatted as base-10 string"}))));
    }
  }

  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "-10");
    }

    auto result = MergeAttributionInteropConfig(std::move(dict), config);

    for (const char* field : kFields) {
      EXPECT_THAT(
          result,
          ErrorIs(HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a non-negative integer "
                                          "formatted as base-10 string"}))));
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigMaxSettableEpsilon) {
  {
    auto result = ParseAttributionInteropConfig(base::Value::Dict());
    EXPECT_THAT(
        result,
        ErrorIs(HasSubstr(
            "[\"max_settable_event_level_epsilon\"]: must be \"inf\" or a "
            "non-negative double formated as a base-10 string")));
  }
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("max_settable_event_level_epsilon", "-1.5");
    EXPECT_THAT(
        MergeAttributionInteropConfig(std::move(dict), config),
        ErrorIs(HasSubstr(
            "[\"max_settable_event_level_epsilon\"]: must be \"inf\" or a "
            "non-negative double formated as a base-10 string")));
  }
}

TEST(AttributionInteropParserTest, InvalidConfigMaxInfoGain) {
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("max_event_level_channel_capacity_navigation", "-1.5");
    EXPECT_THAT(
        MergeAttributionInteropConfig(std::move(dict), config),
        ErrorIs(HasSubstr("[\"max_event_level_channel_capacity_navigation\"]: "
                          "must be \"inf\" or a "
                          "non-negative double formated as a base-10 string")));
  }
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("max_event_level_channel_capacity_event", "-1.5");
    EXPECT_THAT(
        MergeAttributionInteropConfig(std::move(dict), config),
        ErrorIs(HasSubstr("[\"max_event_level_channel_capacity_event\"]: must "
                          "be \"inf\" or a "
                          "non-negative double formated as a base-10 string")));
  }
}

TEST(AttributionInteropParserTest, InvalidConfigMaxTriggerStateCardinality) {
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("max_trigger_state_cardinality", "0");
    EXPECT_THAT(MergeAttributionInteropConfig(std::move(dict), config),
                ErrorIs(HasSubstr(
                    "[\"max_trigger_state_cardinality\"]: must be a positive "
                    "integer formatted as base-10 string")));
  }
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("max_trigger_state_cardinality", "4294967296");
    EXPECT_THAT(
        MergeAttributionInteropConfig(std::move(dict), config),
        ErrorIs(HasSubstr("[\"max_trigger_state_cardinality\"]: must be "
                          "representable by an unsigned 32-bit integer")));
  }
}

TEST(AttributionInteropParserTest, InvalidConfigAggregationCoordinatorOrigins) {
  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("aggregation_coordinator_origins", base::Value());
    EXPECT_THAT(MergeAttributionInteropConfig(std::move(dict), config),
                ErrorIs(HasSubstr(
                    "[\"aggregation_coordinator_origins\"]: must be a list")));
  }

  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("aggregation_coordinator_origins", base::Value::List());
    EXPECT_THAT(
        MergeAttributionInteropConfig(std::move(dict), config),
        ErrorIs(HasSubstr(
            "[\"aggregation_coordinator_origins\"]: must be non-empty")));
  }

  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("aggregation_coordinator_origins",
             base::Value::List().Append(base::Value()));
    EXPECT_THAT(MergeAttributionInteropConfig(std::move(dict), config),
                ErrorIs(HasSubstr("[\"aggregation_coordinator_origins\"][0]: "
                                  "must be a valid, secure origin")));
  }

  {
    AttributionInteropConfig config;
    base::Value::Dict dict;
    dict.Set("aggregation_coordinator_origins",
             base::Value::List().Append("http://c.example"));
    EXPECT_THAT(MergeAttributionInteropConfig(std::move(dict), config),
                ErrorIs(HasSubstr("[\"aggregation_coordinator_origins\"][0]: "
                                  "must be a valid, secure origin")));
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
          ErrorIs(HasSubstr(R"(["reports"]: must be present)")),
      },
      {
          "second_level_errors",
          R"json({
            "reports": [{"foo": null}]
          })json",
          ErrorIs(AllOf(
              HasSubstr(R"(["reports"][0]["report_time"]: must be an integer)"),
              HasSubstr(R"(["reports"][0]["report_url"]: must be a valid URL)"),
              HasSubstr(R"(["reports"][0]["payload"]: required)"))),
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
             ]
          })json",
          ErrorIs(HasSubstr(
              R"(["reports"][1]["report_time"]: must be greater than or equal)")),
      },
      {
          "ok",
          R"json({
            "reports": [{
              "report_time": "123",
              "report_url": "https://a.test/x",
              "payload": "abc"
            }]
          })json",
          ValueIs(Field(
              &AttributionInteropOutput::reports,
              ElementsAre(AllOf(
                  Field(&AttributionInteropOutput::Report::time,
                        base::Time::UnixEpoch() + base::Milliseconds(123)),
                  Field(&AttributionInteropOutput::Report::url,
                        GURL("https://a.test/x")),
                  Field(&AttributionInteropOutput::Report::payload,
                        // `std::ref` needed because `base::Value` isn't
                        // copyable
                        Eq(std::ref(kExpectedPayload))))))),
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
