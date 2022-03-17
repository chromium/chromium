// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <ostream>
#include <sstream>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

bool operator==(const AttributionTriggerAndTime& a,
                const AttributionTriggerAndTime& b) {
  return a.trigger == b.trigger && a.time == b.time;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTriggerAndTime& t) {
  return out << "{time=" << t.time << ",trigger=" << t.trigger << "}";
}

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;

// Pick an arbitrary offset time to test correct handling.
constexpr base::Time kOffsetTime = base::Time::UnixEpoch() + base::Days(5);

TEST(AttributionSimulatorInputParserTest, EmptyInputParses) {
  const char* const kTestCases[] = {
      R"json({})json",
      R"json({"sources":[]})json",
      R"json({"triggers":[]})json",
  };

  for (const char* json : kTestCases) {
    base::Value value = base::test::ParseJson(json);
    std::stringstream error_stream;
    EXPECT_THAT(ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                                error_stream),
                Optional(IsEmpty()))
        << json;
    EXPECT_THAT(error_stream.str(), IsEmpty()) << json;
  }
}

TEST(AttributionSimulatorInputParserTest, ValidSourceParses) {
  constexpr char kJson[] = R"json({"sources": [
    {
      "source_type": "navigation",
      "source_time": 1643235574,
      "reporting_origin": "https://a.r.test",
      "source_origin": "https://a.s.test",
      "registration_config": {
        "source_event_id": "123",
        "destination": "https://a.d.test",
        "expiry": "864000000",
        "priority": "-5",
        "debug_key": "14"
      }
    },
    {
      "source_type": "event",
      "source_time": 1643235573,
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "registration_config": {
        "source_event_id": "456",
        "destination": "https://b.d.test"
      }
    },
    {
      "source_type": "event",
      "source_time": 1643235575,
      "reporting_origin": "https://c.r.test",
      "source_origin": "https://c.s.test",
      "registration_config": {
        "source_event_id": "789",
        "destination": "https://c.d.test",
        "expiry": "864000001",
        "filter_data": {
          "a": [],
          "b": ["c", "d"]
        }
      }
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::stringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          Pair(SourceBuilder(kOffsetTime + base::Seconds(1643235574))
                   .SetSourceType(AttributionSourceType::kNavigation)
                   .SetReportingOrigin(
                       url::Origin::Create(GURL("https://a.r.test")))
                   .SetImpressionOrigin(
                       url::Origin::Create(GURL("https://a.s.test")))
                   .SetSourceEventId(123)
                   .SetConversionOrigin(
                       url::Origin::Create(GURL("https://a.d.test")))
                   .SetExpiry(base::Days(10))
                   .SetPriority(-5)
                   .SetDebugKey(14)
                   .Build(),
               _),
          Pair(SourceBuilder(kOffsetTime + base::Seconds(1643235573))
                   .SetSourceType(AttributionSourceType::kEvent)
                   .SetReportingOrigin(
                       url::Origin::Create(GURL("https://b.r.test")))
                   .SetImpressionOrigin(
                       url::Origin::Create(GURL("https://b.s.test")))
                   .SetSourceEventId(456)
                   .SetConversionOrigin(
                       url::Origin::Create(GURL("https://b.d.test")))
                   .SetExpiry(base::Days(30))   // default
                   .SetPriority(0)              // default
                   .SetDebugKey(absl::nullopt)  // default
                   .Build(),
               _),
          Pair(
              SourceBuilder(kOffsetTime + base::Seconds(1643235575))
                  .SetSourceType(AttributionSourceType::kEvent)
                  .SetReportingOrigin(
                      url::Origin::Create(GURL("https://c.r.test")))
                  .SetImpressionOrigin(
                      url::Origin::Create(GURL("https://c.s.test")))
                  .SetSourceEventId(789)
                  .SetConversionOrigin(
                      url::Origin::Create(GURL("https://c.d.test")))
                  .SetExpiry(base::Days(10))  // rounded to whole number of days
                  .SetPriority(0)             // default
                  .SetDebugKey(absl::nullopt)  // default
                  .SetFilterData(
                      *AttributionFilterData::FromSourceFilterValues({
                          {"a", {}},
                          {"b", {"c", "d"}},
                      }))
                  .Build(),
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, OutputRetainsInputJSON) {
  constexpr char kJson[] = R"json({
    "sources": [
      {
        "source_type": "navigation",
        "source_time": 1643235574,
        "reporting_origin": "https://r.test",
        "source_origin": "https://s.test",
        "registration_config": {
          "source_event_id": "123",
          "destination": "https://d.test",
          "filter_data": {"a": ["b", "c"]},
          "expiry": "864000000",
          "priority": "-5",
          "debug_key": "14"
        }
      }
    ],
    "triggers": [
      {
        "trigger_time": 1643235576,
        "reporting_origin": "https://a.r.test",
        "destination": " https://a.d1.test",
        "registration_config": {
          "trigger_data": "10",
          "event_source_trigger_data": "3",
          "priority": "-5",
          "deduplication_key": "123",
          "debug_key": "14"
        }
      }
    ]})json";

  const base::Value value = base::test::ParseJson(kJson);
  std::stringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(value.Clone(), kOffsetTime, error_stream),
      Optional(ElementsAre(
          Pair(_, base::test::IsJson(
                      value.FindKey("sources")->GetIfList()->front())),
          Pair(_, base::test::IsJson(
                      value.FindKey("triggers")->GetIfList()->front())))));
}

TEST(AttributionSimulatorInputParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"triggers": [
    {
      "trigger_time": 1643235576,
      "reporting_origin": "https://a.r.test",
      "destination": " https://a.d1.test",
      "registration_config": {
        "event_triggers": [
          {
            "trigger_data": "10",
            "priority": "-5",
            "deduplication_key": "123",
            "filters": {
              "x": ["y"]
            },
            "not_filters": {
              "z": []
            }
          },
          {}
        ],
        "debug_key": "14",
        "filters": {
          "a": ["b", "c"],
          "d": []
        }
      }
    },
    {
      "trigger_time": 1643235575,
      "reporting_origin": "https://b.r.test",
      "destination": " https://a.d2.test",
      "registration_config": {}
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::stringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          Pair(
              AttributionTriggerAndTime{
                  .trigger = AttributionTrigger(
                      /*destination_origin=*/
                      url::Origin::Create(GURL("https://a.d1.test")),
                      /*reporting_origin=*/
                      url::Origin::Create(GURL("https://a.r.test")),
                      *AttributionFilterData::FromTriggerFilterValues({
                          {"a", {"b", "c"}},
                          {"d", {}},
                      }),
                      /*debug_key=*/14,
                      {
                          AttributionTrigger::EventTriggerData(
                              /*data=*/10,
                              /*priority=*/-5,
                              /*dedup_key=*/123,
                              /*filters=*/
                              *AttributionFilterData::FromTriggerFilterValues({
                                  {"x", {"y"}},
                              }),
                              /*not_filters=*/
                              *AttributionFilterData::FromTriggerFilterValues({
                                  {"z", {}},
                              })),
                          AttributionTrigger::EventTriggerData(
                              /*data=*/0,
                              /*priority=*/0,
                              /*dedup_key=*/absl::nullopt,
                              /*filters=*/AttributionFilterData(),
                              /*not_filters=*/AttributionFilterData()),
                      },
                      AttributionAggregatableTrigger()),
                  .time = kOffsetTime + base::Seconds(1643235576),
              },
              _),
          Pair(
              AttributionTriggerAndTime{
                  .trigger = AttributionTrigger(
                      /*destination_origin=*/
                      url::Origin::Create(GURL("https://a.d2.test")),
                      /*reporting_origin=*/
                      url::Origin::Create(GURL("https://b.r.test")),
                      AttributionFilterData(),
                      /*debug_key=*/absl::nullopt,
                      /*event_triggers=*/{}, AttributionAggregatableTrigger()),
                  .time = kOffsetTime + base::Seconds(1643235575),
              },
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidSourceAndTriggerParses) {
  constexpr char kJson[] = R"json({
    "sources": [{
      "source_type": "event",
      "source_time": 1643235573,
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "registration_config": {
        "source_event_id": "456",
        "destination": "https://b.d.test"
      }
    }],
    "triggers": [{
      "trigger_time": 1643235575,
      "reporting_origin": "https://b.r.test",
      "destination": " https://a.d2.test",
      "registration_config": {}
    }]
  })json";

  base::Value value = base::test::ParseJson(kJson);
  std::stringstream error_stream;
  EXPECT_THAT(ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                              error_stream),
              Optional(SizeIs(2)));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

struct ParseErrorTestCase {
  const char* expected_failure_substr;
  const char* json;
};

class AttributionSimulatorInputParseErrorTest
    : public testing::TestWithParam<ParseErrorTestCase> {};

TEST_P(AttributionSimulatorInputParseErrorTest, InvalidInputFails) {
  const ParseErrorTestCase& test_case = GetParam();

  base::Value value = base::test::ParseJson(test_case.json);
  std::stringstream error_stream;
  EXPECT_EQ(ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                            error_stream),
            absl::nullopt);

  EXPECT_THAT(error_stream.str(), HasSubstr(test_case.expected_failure_substr));
}

const ParseErrorTestCase kParseErrorTestCases[] = {
    {
        "input root: must be a dictionary",
        R"json(1)json",
    },
    {
        R"(["sources"][0]["source_type"]: must be either)",
        R"json({"sources": [{
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["source_time"]: must be an integer number of)",
        R"json({"sources": [{
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "source_origin": "https://a.s.test",
          "reporting_origin": "http://r.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["source_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]: must be present)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]: must be a dictionary)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": ""
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["source_event_id"]: must be a uint64 formatted)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["destination"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
          }
        }]})json",
    },
    {
        R"(["sources"][0]["source_type"]: must be either)",
        R"json({"sources": [{
          "source_type": "NAVIGATION",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["expiry"]: must be a positive number of)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "expiry": "-5"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["priority"]: must be an int64)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "priority": "x"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["source_event_id"]: must be a uint64 formatted)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "x",
            "destination": "https://a.d.test",
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["filter_data"]: must be a dictionary)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": ""
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["filter_data"]["a"]: must be a list)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": {
              "a": "x"
            }
          }
        }]})json",
    },
    {
        R"(["sources"][0]["registration_config"]["filter_data"]["a"][0]: must be a string)",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "registration_config": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": {
              "a": [5]
            }
          }
        }]})json",
    },
    {
        R"(["sources"]: must be a list)",
        R"json({"sources": ""})json",
    },
    {
        R"(["triggers"][0]["registration_config"]: must be present)",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
        }]})json",
    },
    {
        R"(["triggers"][0]["registration_config"]: must be a dictionary)",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
          "registration_config": ""
        }]})json",
    },
    {
        R"(["triggers"][0]["trigger_time"]: must be an integer number of)",
        R"json({"triggers": [{
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
          "registration_config": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["destination"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "registration_config": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "destination": " https://a.d1.test",
          "registration_config": {}
        }]})json",
    },
    {
        R"(["triggers"]: must be a list)",
        R"json({"triggers": ""})json",
    },
    {
        R"(["triggers"][0]["registration_config"]["event_triggers"]: must be a list)",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
          "registration_config": {
            "event_triggers": 1
          }
        }]})json",
    },
};

INSTANTIATE_TEST_SUITE_P(AttributionSimulatorInputParserInvalidInputs,
                         AttributionSimulatorInputParseErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

}  // namespace
}  // namespace content
