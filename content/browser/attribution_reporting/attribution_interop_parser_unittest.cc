// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

bool operator==(const AttributionTriggerAndTime& a,
                const AttributionTriggerAndTime& b) {
  return a.trigger == b.trigger && a.time == b.time;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTriggerAndTime& t) {
  return out << "{time=" << t.time << ",trigger=" << t.trigger << "}";
}

bool operator==(const AttributionSimulationEvent& a,
                const AttributionSimulationEvent& b) {
  return a.event == b.event && a.debug_permission == b.debug_permission;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionSimulationEvent& e) {
  out << "{event=";
  absl::visit([&](const auto& event) { out << event; }, e.event);
  return out << ",debug_permission=" << e.debug_permission << "}";
}

bool operator==(AttributionConfig::RateLimitConfig a,
                AttributionConfig::RateLimitConfig b) {
  const auto tie = [](AttributionConfig::RateLimitConfig config) {
    return std::make_tuple(
        config.time_window, config.max_source_registration_reporting_origins,
        config.max_attribution_reporting_origins, config.max_attributions);
  };
  return tie(a) == tie(b);
}

bool operator==(AttributionConfig::EventLevelLimit a,
                AttributionConfig::EventLevelLimit b) {
  const auto tie = [](AttributionConfig::EventLevelLimit config) {
    return std::make_tuple(config.navigation_source_trigger_data_cardinality,
                           config.event_source_trigger_data_cardinality,
                           config.navigation_source_randomized_response_rate,
                           config.event_source_randomized_response_rate,
                           config.max_reports_per_destination,
                           config.max_attributions_per_navigation_source,
                           config.max_attributions_per_event_source);
  };
  return tie(a) == tie(b);
}

bool operator==(AttributionConfig::AggregateLimit a,
                AttributionConfig::AggregateLimit b) {
  const auto tie = [](AttributionConfig::AggregateLimit config) {
    return std::make_tuple(config.max_reports_per_destination,
                           config.aggregatable_budget_per_source,
                           config.min_delay, config.delay_span);
  };
  return tie(a) == tie(b);
}

bool operator==(AttributionConfig a, AttributionConfig b) {
  const auto tie = [](AttributionConfig config) {
    return std::make_tuple(
        config.max_sources_per_origin, config.source_event_id_cardinality,
        config.rate_limit, config.event_level_limit, config.aggregate_limit);
  };
  return tie(a) == tie(b);
}

namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

using ::attribution_reporting::SuitableOrigin;

// Pick an arbitrary offset time to test correct handling.
constexpr base::Time kOffsetTime = base::Time::UnixEpoch() + base::Days(5);

TEST(AttributionInteropParserTest, EmptyInputParses) {
  const char* const kTestCases[] = {
      R"json({})json",
      R"json({"sources":[]})json",
      R"json({"triggers":[]})json",
  };

  for (const char* json : kTestCases) {
    base::Value::Dict value = base::test::ParseJsonDict(json);
    auto result = ParseAttributionInteropInput(std::move(value), kOffsetTime);
    ASSERT_TRUE(result.has_value()) << json;
    EXPECT_THAT(*result, IsEmpty()) << json;
  }
}

TEST(AttributionInteropParserTest, ValidSourceParses) {
  constexpr char kJson[] = R"json({"sources": [
    {
      "timestamp": "1643235573123",
      "registration_request": {
        "source_type": "navigation",
        "attribution_src_url": "https://a.r.test",
        "source_origin": "https://a.s.test"
      },
      "responses": [{
        "url": "https://a.r.test",
        "debug_permission": true,
        "response": {
          "Attribution-Reporting-Register-Source": {
            "destination": "https://a.d.test"
          }
        }
      }]
    },
    {
      "timestamp": "1643235574123",
      "registration_request": {
        "source_type": "event",
        "attribution_src_url": "https://b.r.test",
        "source_origin": "https://b.s.test",
      },
      "responses": [{
        "url": "https://b.r.test",
        "response": {
          "Attribution-Reporting-Register-Source": {
            "destination": "https://b.d.test"
          }
        }
      }]
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  auto result = ParseAttributionInteropInput(std::move(value), kOffsetTime);

  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_EQ(result->size(), 2u);

  const auto* source1 = absl::get_if<StorableSource>(&result->front().event);
  ASSERT_TRUE(source1);

  const auto* source2 = absl::get_if<StorableSource>(&result->back().event);
  ASSERT_TRUE(source2);

  EXPECT_EQ(source1->common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235573123));
  EXPECT_EQ(source1->common_info().source_type(),
            attribution_reporting::mojom::SourceType::kNavigation);
  EXPECT_EQ(source1->common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(source1->common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://a.s.test"));
  EXPECT_THAT(source1->common_info().destination_sites().destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
  EXPECT_FALSE(source1->is_within_fenced_frame());
  EXPECT_TRUE(result->front().debug_permission);

  EXPECT_EQ(source2->common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235574123));
  EXPECT_EQ(source2->common_info().source_type(),
            attribution_reporting::mojom::SourceType::kEvent);
  EXPECT_EQ(source2->common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://b.r.test"));
  EXPECT_EQ(source2->common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://b.s.test"));
  EXPECT_THAT(source2->common_info().destination_sites().destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
  EXPECT_FALSE(source2->is_within_fenced_frame());
  EXPECT_FALSE(result->back().debug_permission);
}

TEST(AttributionInteropParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"triggers": [
    {
      "timestamp": "1643235575123",
      "registration_request": {
        "attribution_src_url": "https://a.r.test",
        "destination_origin": " https://b.d.test",
      },
      "responses": [{
        "url": "https://a.r.test",
        "debug_permission": true,
        "response": {
          "Attribution-Reporting-Register-Trigger": {}
        }
      }]
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  auto result = ParseAttributionInteropInput(std::move(value), kOffsetTime);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);

  const auto* trigger =
      absl::get_if<AttributionTriggerAndTime>(&result->front().event);
  ASSERT_TRUE(trigger);

  EXPECT_EQ(trigger->time, kOffsetTime + base::Milliseconds(1643235575123));
  EXPECT_EQ(trigger->trigger.reporting_origin(),
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(trigger->trigger.destination_origin(),
            *SuitableOrigin::Deserialize("https://b.d.test"));
  EXPECT_EQ(trigger->trigger.attestation(), absl::nullopt);
  EXPECT_FALSE(trigger->trigger.is_within_fenced_frame());
  EXPECT_TRUE(result->front().debug_permission);
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
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr(test_case.expected_failure_substr));
}

const ParseErrorTestCase kParseErrorTestCases[] = {
    {
        R"(["sources"]: must be a list)",
        R"json({"sources": ""})json",
    },
    {
        R"(["sources"][0]: must be a dictionary)",
        R"json({"sources": [""]})json",
    },
    {
        R"(["sources"][0]["timestamp"]: must be an integer number of)",
        R"json({"sources": [{}]})json",
    },
    {
        R"(["sources"][0]["registration_request"]: must be present)",
        R"json({"sources":[{}]})json",
    },
    {
        R"(["sources"][0]["registration_request"]: must be a dictionary)",
        R"json({"sources": [{
          "registration_request": ""
        }]})json",
    },
    {
        R"(["sources"][0]["registration_request"]["source_type"]: must be either)",
        R"json({"sources": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["sources"][0]["registration_request"]["attribution_src_url"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["sources"][0]["registration_request"]["attribution_src_url"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "registration_request": {
            "attribution_src_url": "http://r.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_request"]["source_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["sources"][0]["registration_request"]["source_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "registration_request": {
            "source_origin": "http://s.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["responses"]: must be present)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["responses"]: must be a list)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": ""
        }]})json",
    },
    {
        R"(["sources"][0]["responses"]: must have size 1)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{}, {}]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [""]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["url"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{}]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["url"]: must match https://a.r.test)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://b.r.test"
          }]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["response"]: must be present)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test"
          }]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["response"]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": ""
          }]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Source"]: must be present)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {}
          }]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Source"]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Source": ""
            }
          }]
        }]})json",
    },
    {
        R"(["sources"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Source"]: kDestinationMissing)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "navigation",
            "attribution_src_url": "https://a.r.test",
            "source_origin": "https://a.s.test",
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
        R"(["sources"][0]["registration_request"]["source_type"]: must be either)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "registration_request": {
            "source_type": "NAVIGATION"
          }
        }]})json",
    },
    {
        R"(["triggers"]: must be a list)",
        R"json({"triggers": ""})json",
    },
    {
        R"(["triggers"][0]["timestamp"]: must be an integer number of)",
        R"json({"triggers": [{}]})json",
    },
    {
        R"(["triggers"][0]["registration_request"]["destination_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["registration_request"]["attribution_src_url"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "registration_request": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Trigger"]: must be present)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "registration_request": {
            "destination_origin": "https://a.d1.test",
            "attribution_src_url": "https://a.r.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {}
          }]
        }]})json",
    },
    {
        R"(["triggers"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Trigger"]: must be a dictionary)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "registration_request": {
            "destination_origin": "https://a.d1.test",
            "attribution_src_url": "https://a.r.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Trigger": ""
            }
          }]
        }]})json",
    },
    {
        R"(["triggers"][0]["responses"][0]["response"]["Attribution-Reporting-Register-Trigger"]: kFiltersWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "registration_request": {
            "destination_origin": "https://a.d1.test",
            "attribution_src_url": "https://a.r.test"
          },
          "responses": [{
            "url": "https://a.r.test",
            "response": {
              "Attribution-Reporting-Register-Trigger": {
                "filters": ""
              }
            }
          }]
        }]})json",
    },
    {
        R"(["triggers"][1]["timestamp"]: must be distinct from all others: 1643235576000)",
        R"json({"triggers": [
          {
            "timestamp": "1643235576000",
            "registration_request": {
              "destination_origin": "https://a.d1.test",
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
            "timestamp": "1643235576000",
            "registration_request": {
              "destination_origin": "https://a.d1.test",
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
  const struct {
    const char* json;
    bool required;
    AttributionConfig expected;
  } kTestCases[] = {
      {R"json({})json", false, AttributionConfig()},
      {R"json({"max_sources_per_origin":"100"})json", false,
       AttributionConfig{.max_sources_per_origin = 100}},
      {R"json({"max_destinations_per_source_site_reporting_origin":"100"})json",
       false,
       AttributionConfig{.max_destinations_per_source_site_reporting_origin =
                             100}},
      {R"json({"source_event_id_cardinality":"0"})json", false,
       AttributionConfig{.source_event_id_cardinality = absl::nullopt}},
      {R"json({"source_event_id_cardinality":"10"})json", false,
       AttributionConfig{.source_event_id_cardinality = 10}},
      {R"json({"rate_limit_time_window":"30"})json", false,
       AttributionConfig{.rate_limit = {.time_window = base::Days(30)}}},
      {R"json({"rate_limit_max_source_registration_reporting_origins":"10"})json",
       false,
       AttributionConfig{
           .rate_limit = {.max_source_registration_reporting_origins = 10}}},
      {R"json({"rate_limit_max_attribution_reporting_origins":"10"})json",
       false,
       AttributionConfig{
           .rate_limit = {.max_attribution_reporting_origins = 10}}},
      {R"json({"rate_limit_max_attributions":"10"})json", false,
       AttributionConfig{.rate_limit = {.max_attributions = 10}}},
      {R"json({"navigation_source_trigger_data_cardinality":"10"})json", false,
       AttributionConfig{
           .event_level_limit = {.navigation_source_trigger_data_cardinality =
                                     10}}},
      {R"json({"event_source_trigger_data_cardinality":"10"})json", false,
       AttributionConfig{
           .event_level_limit = {.event_source_trigger_data_cardinality = 10}}},
      {R"json({"navigation_source_randomized_response_rate":0.2})json", false,
       AttributionConfig{
           .event_level_limit = {.navigation_source_randomized_response_rate =
                                     0.2}}},
      {R"json({"event_source_randomized_response_rate":0.2})json", false,
       AttributionConfig{
           .event_level_limit = {.event_source_randomized_response_rate =
                                     0.2}}},
      {R"json({"max_event_level_reports_per_destination":"10"})json", false,
       AttributionConfig{
           .event_level_limit = {.max_reports_per_destination = 10}}},
      {R"json({"max_attributions_per_navigation_source":"10"})json", false,
       AttributionConfig{
           .event_level_limit = {.max_attributions_per_navigation_source =
                                     10}}},
      {R"json({"max_attributions_per_event_source":"10"})json", false,
       AttributionConfig{
           .event_level_limit = {.max_attributions_per_event_source = 10}}},
      {R"json({"max_aggregatable_reports_per_destination":"10"})json", false,
       AttributionConfig{
           .aggregate_limit = {.max_reports_per_destination = 10}}},
      {R"json({"aggregatable_budget_per_source":"100"})json", false,
       AttributionConfig{
           .aggregate_limit = {.aggregatable_budget_per_source = 100}}},
      {R"json({"aggregatable_report_min_delay":"0"})json", false,
       AttributionConfig{.aggregate_limit = {.min_delay = base::TimeDelta()}}},
      {R"json({"aggregatable_report_delay_span":"0"})json", false,
       AttributionConfig{.aggregate_limit = {.delay_span = base::TimeDelta()}}},
      {R"json({
        "max_sources_per_origin":"10",
        "max_destinations_per_source_site_reporting_origin":"10",
        "source_event_id_cardinality":"100",
        "rate_limit_time_window":"10",
        "rate_limit_max_source_registration_reporting_origins":"20",
        "rate_limit_max_attribution_reporting_origins":"15",
        "rate_limit_max_attributions":"10",
        "navigation_source_trigger_data_cardinality":"100",
        "event_source_trigger_data_cardinality":"10",
        "navigation_source_randomized_response_rate":0.2,
        "event_source_randomized_response_rate":0.1,
        "max_event_level_reports_per_destination":"10",
        "max_attributions_per_navigation_source":"5",
        "max_attributions_per_event_source":"1",
        "max_aggregatable_reports_per_destination":"10",
        "aggregatable_budget_per_source":"1000",
        "aggregatable_report_min_delay":"10",
        "aggregatable_report_delay_span":"20"
      })json",
       true,
       AttributionConfig{
           .max_sources_per_origin = 10,
           .source_event_id_cardinality = 100,
           .max_destinations_per_source_site_reporting_origin = 10,
           .rate_limit = {.time_window = base::Days(10),
                          .max_source_registration_reporting_origins = 20,
                          .max_attribution_reporting_origins = 15,
                          .max_attributions = 10},
           .event_level_limit = {.navigation_source_trigger_data_cardinality =
                                     100,
                                 .event_source_trigger_data_cardinality = 10,
                                 .navigation_source_randomized_response_rate =
                                     0.2,
                                 .event_source_randomized_response_rate = 0.1,
                                 .max_reports_per_destination = 10,
                                 .max_attributions_per_navigation_source = 5,
                                 .max_attributions_per_event_source = 1},
           .aggregate_limit = {.max_reports_per_destination = 10,
                               .aggregatable_budget_per_source = 1000,
                               .min_delay = base::Minutes(10),
                               .delay_span = base::Minutes(20)}}},
  };

  for (const auto& test_case : kTestCases) {
    base::Value::Dict json = base::test::ParseJsonDict(test_case.json);
    if (test_case.required) {
      auto result = ParseAttributionConfig(json);
      EXPECT_TRUE(result.has_value()) << json;
      EXPECT_EQ(result, test_case.expected) << json;
    } else {
      AttributionConfig config;
      EXPECT_EQ("", MergeAttributionConfig(json, config)) << json;
      EXPECT_EQ(config, test_case.expected) << json;
    }
  }
}

TEST(AttributionInteropParserTest, InvalidConfigPositiveIntegers) {
  const char* const kFields[] = {
      "max_sources_per_origin",
      "max_destinations_per_source_site_reporting_origin",
      "rate_limit_time_window",
      "rate_limit_max_source_registration_reporting_origins",
      "rate_limit_max_attribution_reporting_origins",
      "rate_limit_max_attributions",
      "navigation_source_trigger_data_cardinality",
      "event_source_trigger_data_cardinality",
      "max_event_level_reports_per_destination",
      "max_attributions_per_navigation_source",
      "max_attributions_per_event_source",
      "max_aggregatable_reports_per_destination",
      "aggregatable_budget_per_source",
  };

  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    ASSERT_FALSE(result.has_value());

    for (const char* field : kFields) {
      EXPECT_THAT(
          result.error(),
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a positive integer formatted as base-10 string"})))
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
      "source_event_id_cardinality",
      "aggregatable_report_min_delay",
      "aggregatable_report_delay_span",
  };

  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    ASSERT_FALSE(result.has_value());

    for (const char* field : kFields) {
      EXPECT_THAT(result.error(),
                  HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a non-negative integer "
                                          "formatted as base-10 string"})))
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

TEST(AttributionInteropParserTest, InvalidConfigRandomizedResponseRates) {
  const char* const kFields[] = {
      "navigation_source_randomized_response_rate",
      "event_source_randomized_response_rate",
  };

  {
    auto result = ParseAttributionConfig(base::Value::Dict());
    ASSERT_FALSE(result.has_value());

    for (const char* field : kFields) {
      EXPECT_THAT(
          result.error(),
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a double between 0 and 1 formatted as string"})))
          << field;
    }
  }

  {
    AttributionConfig config;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "1.5");
    }

    std::string error = MergeAttributionConfig(dict, config);

    for (const char* field : kFields) {
      EXPECT_THAT(
          error,
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a double between 0 and 1 formatted as string"})))
          << field;
    }
  }
}

}  // namespace
}  // namespace content
