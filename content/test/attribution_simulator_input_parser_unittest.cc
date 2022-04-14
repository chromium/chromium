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
      "timestamp": 1643235574,
      "source_type": "navigation",
      "reporting_origin": "https://a.r.test",
      "source_origin": "https://a.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "123",
        "destination": "https://a.d.test",
        "expiry": "864000000",
        "priority": "-5",
        "debug_key": "14"
      }
    },
    {
      "timestamp": 1643235573,
      "source_type": "event",
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "456",
        "destination": "https://b.d.test"
      }
    },
    {
      "timestamp": 1643235575,
      "source_type": "event",
      "reporting_origin": "https://c.r.test",
      "source_origin": "https://c.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "789",
        "destination": "https://c.d.test",
        "expiry": "864000001",
        "filter_data": {
          "a": [],
          "b": ["c", "d"]
        }
      }
    },
    {
      "timestamp": 1643235576,
      "source_type": "event",
      "reporting_origin": "https://c.r.test",
      "source_origin": "https://c.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "789",
        "destination": "https://c.d.test",
        "expiry": "864000001"
      },
      "Attribution-Reporting-Register-Aggregatable-Source": [{
        "id": "a",
        "key_piece": "0x1"
      }]
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
              _),
          Pair(
              SourceBuilder(kOffsetTime + base::Seconds(1643235576))
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
                  .SetAggregatableSource(*AttributionAggregatableSource::Create(
                      AggregatableSourceProtoBuilder()
                          .AddKey("a", AggregatableKeyProtoBuilder()
                                           .SetHighBits(0)
                                           .SetLowBits(1)
                                           .Build())
                          .Build()))
                  .Build(),
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, OutputRetainsInputJSON) {
  constexpr char kJson[] = R"json({
    "sources": [
      {
        "timestamp": 1643235574,
        "source_type": "navigation",
        "reporting_origin": "https://r.test",
        "source_origin": "https://s.test",
        "Attribution-Reporting-Register-Source": {
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
        "timestamp": 1643235576,
        "reporting_origin": "https://a.r.test",
        "destination_origin": " https://a.d1.test",
        "trigger_data": "10",
        "event_source_trigger_data": "3",
        "priority": "-5",
        "deduplication_key": "123",
        "debug_key": "14"
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
      "timestamp": 1643235576,
      "reporting_origin": "https://a.r.test",
      "destination_origin": " https://a.d1.test",
      "Attribution-Reporting-Register-Event-Trigger": [
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
      "Attribution-Reporting-Trigger-Debug-Key": "14",
      "Attribution-Reporting-Filters": {
        "a": ["b", "c"],
        "d": []
      }
    },
    {
      "timestamp": 1643235575,
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test"
    },
    {
      "timestamp": 1643235574,
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test",
      "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [{
        "source_keys": ["a"],
        "key_piece": "0x1"
      }],
      "Attribution-Reporting-Register-Aggregatable-Values": {"a": 1}
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::stringstream error_stream;

  std::vector<blink::mojom::AttributionAggregatableTriggerDataPtr>
      aggregatable_trigger_data;
  aggregatable_trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/0,
                                                        /*low_bits=*/1),
          std::vector<std::string>{"a"},
          blink::mojom::AttributionFilterData::New(),
          blink::mojom::AttributionFilterData::New()));

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
                      /*event_triggers=*/{},
                      *AttributionAggregatableTrigger::FromMojo(
                          blink::mojom::AttributionAggregatableTrigger::New(
                              std::move(aggregatable_trigger_data),
                              AttributionAggregatableTrigger::Values{
                                  {"a", 1}}))),
                  .time = kOffsetTime + base::Seconds(1643235574),
              },
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidSourceAndTriggerParses) {
  constexpr char kJson[] = R"json({
    "sources": [{
      "timestamp": 1643235573,
      "source_type": "event",
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "456",
        "destination": "https://b.d.test"
      }
    }],
    "triggers": [{
      "timestamp": 1643235575,
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test"
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
          "timestamp": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["timestamp"]: must be an integer number of)",
        R"json({"sources": [{
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "source_origin": "https://a.s.test",
          "reporting_origin": "http://r.test"
        }]})json",
    },
    {
        R"(["sources"][0]["source_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: must be present)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": ""
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["source_event_id"]: must be a uint64 formatted)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["destination"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["source_type"]: must be either)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "NAVIGATION",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["expiry"]: must be a positive number of)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "expiry": "-5"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["priority"]: must be an int64)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "priority": "x"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["source_event_id"]: must be a uint64 formatted)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "x",
            "destination": "https://a.d.test"
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["filter_data"]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": ""
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["filter_data"]["a"]: must be a list)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": {
              "a": "x"
            }
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]["filter_data"]["a"][0]: must be a string)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "filter_data": {
              "a": [5]
            }
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Aggregatable-Source"]: must be a list)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          },
          "Attribution-Reporting-Register-Aggregatable-Source": ""
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Aggregatable-Source"][0]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          },
          "Attribution-Reporting-Register-Aggregatable-Source": [5]
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Aggregatable-Source"][0]["id"]: must be a string)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          },
          "Attribution-Reporting-Register-Aggregatable-Source": [{"id": 5}]
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Aggregatable-Source"][0]["key_piece"]: must be a uint128 formatted as a base-16 string)",
        R"json({"sources": [{
          "timestamp": 1643235574,
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test"
          },
          "Attribution-Reporting-Register-Aggregatable-Source": [{
            "id": "a",
            "key_piece": "0xG"
          }]
        }]})json",
    },
    {
        R"(["sources"]: must be a list)",
        R"json({"sources": ""})json",
    },
    {
        R"(["triggers"][0]["timestamp"]: must be an integer number of)",
        R"json({"triggers": [{
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test"
        }]})json",
    },
    {
        R"(["triggers"][0]["destination_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test"
        }]})json",
    },
    {
        R"(["triggers"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "destination_origin": " https://a.d1.test"
        }]})json",
    },
    {
        R"(["triggers"]: must be a list)",
        R"json({"triggers": ""})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Event-Trigger"]: must be a list)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Event-Trigger": 1
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"]: must be a list)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": 5
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"][0]: must be a dictionary)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [ 5 ]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"][0]["source_keys"]: must be present)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [{}]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"][0]["source_keys"]: must be a list)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [{
            "source_keys": "a"
          }]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"][0]["source_keys"][0]: must be a string)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [{
            "source_keys": [ 5 ]
          }]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"][0]["key_piece"]: must be a uint128 formatted as a base-16 string)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [{
            "source_keys": [ "a" ],
            "key_piece": "0xG"
          }]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Values"]: must be a dictionary)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Values": 5
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Values"]["a"]: must be a positive integer)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Values": {
            "a": -5
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Event-Trigger"][0]: must be a dictionary)",
        R"json({"triggers":[{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Event-Trigger":[true]
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Trigger-Data"]: must be present)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Values": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Aggregatable-Values"]: must be present)",
        R"json({"triggers": [{
          "timestamp": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Aggregatable-Trigger-Data": []
        }]})json",
    }};

INSTANTIATE_TEST_SUITE_P(AttributionSimulatorInputParserInvalidInputs,
                         AttributionSimulatorInputParseErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

}  // namespace
}  // namespace content
