// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <ostream>
#include <sstream>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
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

bool operator==(const AttributionSimulatorCookie& a,
                const AttributionSimulatorCookie& b) {
  return a.cookie.HasEquivalentDataMembers(b.cookie) &&
         a.source_url == b.source_url;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionSimulatorCookie& c) {
  return out << "{source_url=" << c.source_url
             << ",cookie=" << c.cookie.DebugString() << "}";
}

bool operator==(const AttributionDataClear& a, const AttributionDataClear& b) {
  return a.time == b.time && a.delete_begin == b.delete_begin &&
         a.delete_end == b.delete_end && a.origins == b.origins;
}

std::ostream& operator<<(std::ostream& out, const AttributionDataClear& c) {
  out << "{time=" << c.time << ",delete_begin=" << c.delete_begin
      << ",delete_end=" << c.delete_end << ",origins=";

  if (c.origins.has_value()) {
    out << "[";

    const char* separator = "";
    for (const url::Origin& origin : *c.origins) {
      out << separator << origin;
      separator = ", ";
    }

    out << "]";
  } else {
    out << "null";
  }

  return out;
}

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;

using AttributionFilters = ::attribution_reporting::Filters;

using ::attribution_reporting::SuitableOrigin;

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
    std::ostringstream error_stream;
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
      "timestamp": "1643235574123",
      "source_type": "navigation",
      "reporting_origin": "https://a.r.test",
      "source_origin": "https://a.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "123",
        "destination": "https://a.d.test",
        "expiry": "864000",
        "event_report_window": "864000",
        "aggregatable_report_window": "864000",
        "priority": "-5",
        "debug_key": "14",
        "debug_reporting": true
      }
    },
    {
      "timestamp": "1643235573123",
      "source_type": "event",
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "Attribution-Reporting-Register-Source": {
        "destination": "https://b.d.test"
      }
    },
    {
      "timestamp": "1643235575123",
      "source_type": "event",
      "reporting_origin": "https://c.r.test",
      "source_origin": "https://c.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "789",
        "destination": "https://c.d.test",
        "expiry": "864001",
        "event_report_window": "864001",
        "aggregatable_report_window": "864001",
        "filter_data": {
          "a": [],
          "b": ["c", "d"]
        }
      }
    },
    {
      "timestamp": "1643235576123",
      "source_type": "event",
      "reporting_origin": "https://c.r.test",
      "source_origin": "https://c.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "789",
        "destination": "https://c.d.test",
        "expiry": "864001",
        "event_report_window": "691201",
        "aggregatable_report_window": "432001",
        "aggregation_keys": {
          "a": "0x1"
        }
      }
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          Pair(SourceBuilder(kOffsetTime + base::Milliseconds(1643235574123))
                   .SetSourceType(AttributionSourceType::kNavigation)
                   .SetReportingOrigin(
                       *SuitableOrigin::Deserialize("https://a.r.test"))
                   .SetSourceOrigin(
                       *SuitableOrigin::Deserialize("https://a.s.test"))
                   .SetSourceEventId(123)
                   .SetDestinationOrigin(
                       *SuitableOrigin::Deserialize("https://a.d.test"))
                   .SetExpiry(base::Days(10))
                   .SetEventReportWindow(base::Days(10))
                   .SetAggregatableReportWindow(base::Days(10))
                   .SetPriority(-5)
                   .SetDebugKey(14)
                   .SetDebugReporting(true)
                   .Build(),
               _),
          Pair(SourceBuilder(kOffsetTime + base::Milliseconds(1643235573123))
                   .SetSourceType(AttributionSourceType::kEvent)
                   .SetReportingOrigin(
                       *SuitableOrigin::Deserialize("https://b.r.test"))
                   .SetSourceOrigin(
                       *SuitableOrigin::Deserialize("https://b.s.test"))
                   .SetSourceEventId(0)  // default
                   .SetDestinationOrigin(
                       *SuitableOrigin::Deserialize("https://b.d.test"))
                   .SetExpiry(base::Days(30))                    // default
                   .SetEventReportWindow(base::Days(30))         // default
                   .SetAggregatableReportWindow(base::Days(30))  // default
                   .SetPriority(0)                               // default
                   .SetDebugKey(absl::nullopt)                   // default
                   .Build(),
               _),
          Pair(
              SourceBuilder(kOffsetTime + base::Milliseconds(1643235575123))
                  .SetSourceType(AttributionSourceType::kEvent)
                  .SetReportingOrigin(
                      *SuitableOrigin::Deserialize("https://c.r.test"))
                  .SetSourceOrigin(
                      *SuitableOrigin::Deserialize("https://c.s.test"))
                  .SetSourceEventId(789)
                  .SetDestinationOrigin(
                      *SuitableOrigin::Deserialize("https://c.d.test"))
                  .SetExpiry(base::Days(10))  // rounded to whole number of days
                  .SetEventReportWindow(
                      base::Days(10))  // rounded to whole number of days
                  .SetAggregatableReportWindow(
                      base::Days(10))  // rounded to whole number of days
                  .SetPriority(0)      // default
                  .SetDebugKey(absl::nullopt)  // default
                  .SetDebugReporting(false)    // default
                  .SetFilterData(*attribution_reporting::FilterData::Create({
                      {"a", {}},
                      {"b", {"c", "d"}},
                  }))
                  .Build(),
              _),
          Pair(
              SourceBuilder(kOffsetTime + base::Milliseconds(1643235576123))
                  .SetSourceType(AttributionSourceType::kEvent)
                  .SetReportingOrigin(
                      *SuitableOrigin::Deserialize("https://c.r.test"))
                  .SetSourceOrigin(
                      *SuitableOrigin::Deserialize("https://c.s.test"))
                  .SetSourceEventId(789)
                  .SetDestinationOrigin(
                      *SuitableOrigin::Deserialize("https://c.d.test"))
                  .SetExpiry(base::Days(10))  // rounded to whole number of days
                  .SetEventReportWindow(
                      base::Days(8))  // rounded to whole number of days
                  .SetAggregatableReportWindow(
                      base::Days(5))  // rounded to whole number of days
                  .SetPriority(0)     // default
                  .SetDebugKey(absl::nullopt)  // default
                  .SetDebugReporting(false)    // default
                  .SetAggregationKeys(
                      *attribution_reporting::AggregationKeys::FromKeys(
                          {{"a", 1}}))
                  .Build(),
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, OutputRetainsInputJSON) {
  constexpr char kJson[] = R"json({
    "sources": [
      {
        "timestamp": "1643235574123",
        "source_type": "navigation",
        "reporting_origin": "https://r.test",
        "source_origin": "https://s.test",
        "Attribution-Reporting-Register-Source": {
          "source_event_id": "123",
          "destination": "https://d.test",
          "filter_data": {"a": ["b", "c"]},
          "expiry": "864000",
          "event_report_window": "864000",
          "aggregatable_report_window": "864000",
          "priority": "-5",
          "debug_key": "14"
        }
      }
    ],
    "triggers": [
      {
        "timestamp": "1643235576123",
        "reporting_origin": "https://a.r.test",
        "destination_origin": " https://a.d1.test",
        "Attribution-Reporting-Register-Trigger": {
          "trigger_data": "10",
          "event_source_trigger_data": "3",
          "priority": "-5",
          "deduplication_key": "123",
          "debug_key": "14"
        }
      }
    ]})json";

  const base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
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
      "timestamp": "1643235576123",
      "reporting_origin": "https://a.r.test",
      "destination_origin": " https://a.d1.test",
      "Attribution-Reporting-Register-Trigger": {
        "event_trigger_data": [
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
        },
        "not_filters": {
          "e": ["f"]
        }
      }
    },
    {
      "timestamp": "1643235575123",
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test",
      "Attribution-Reporting-Register-Trigger": {}
    },
    {
      "timestamp": "1643235574123",
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test",
      "Attribution-Reporting-Register-Trigger": {
        "aggregatable_trigger_data": [{
          "source_keys": ["a"],
          "key_piece": "0x1"
        }],
        "aggregatable_values": {"a": 1},
        "aggregatable_deduplication_key": "789",
        "debug_reporting": true
      }
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;

  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          Pair(
              AttributionTriggerAndTime{
                  .trigger = AttributionTrigger(
                      /*reporting_origin=*/
                      *SuitableOrigin::Deserialize("https://a.r.test"),
                      attribution_reporting::TriggerRegistration(
                          /*filters=*/
                          *AttributionFilters::Create({
                              {"a", {"b", "c"}},
                              {"d", {}},
                          }),
                          /*not_filters=*/
                          *AttributionFilters::Create({
                              {"e", {"f"}},
                          }),
                          /*debug_key=*/14,
                          /*aggregatable_dedup_key=*/absl::nullopt,
                          *attribution_reporting::EventTriggerDataList::Create({
                              attribution_reporting::EventTriggerData(
                                  /*data=*/10,
                                  /*priority=*/-5,
                                  /*dedup_key=*/123,
                                  /*filters=*/
                                  *AttributionFilters::Create({
                                      {"x", {"y"}},
                                  }),
                                  /*not_filters=*/
                                  *AttributionFilters::Create({
                                      {"z", {}},
                                  })),
                              attribution_reporting::EventTriggerData(
                                  /*data=*/0,
                                  /*priority=*/0,
                                  /*dedup_key=*/absl::nullopt,
                                  /*filters=*/AttributionFilters(),
                                  /*not_filters=*/AttributionFilters()),
                          }),
                          /*aggregatable_trigger_data=*/
                          attribution_reporting::AggregatableTriggerDataList(),
                          /*aggregatable_values=*/
                          attribution_reporting::AggregatableValues(),
                          /*debug_reporting=*/false,
                          ::aggregation_service::mojom::AggregationCoordinator::
                              kDefault),
                      /*destination_origin=*/
                      *SuitableOrigin::Deserialize("https://a.d1.test"),
                      /*is_within_fenced_frame=*/false),
                  .time = kOffsetTime + base::Milliseconds(1643235576123),
              },
              _),
          Pair(
              AttributionTriggerAndTime{
                  .trigger = AttributionTrigger(
                      /*reporting_origin=*/
                      *SuitableOrigin::Deserialize("https://b.r.test"),
                      attribution_reporting::TriggerRegistration(
                          /*filters=*/AttributionFilters(),
                          /*not_filters=*/AttributionFilters(),
                          /*debug_key=*/absl::nullopt,
                          /*aggregatable_dedup_key=*/absl::nullopt,
                          /*event_triggers=*/
                          attribution_reporting::EventTriggerDataList(),
                          /*aggregatable_trigger_data=*/
                          attribution_reporting::AggregatableTriggerDataList(),
                          /*aggregatable_values=*/
                          attribution_reporting::AggregatableValues(),
                          /*debug_reporting=*/false,
                          ::aggregation_service::mojom::AggregationCoordinator::
                              kDefault),
                      /*destination_origin=*/
                      *SuitableOrigin::Deserialize("https://a.d2.test"),
                      /*is_within_fenced_frame=*/false),
                  .time = kOffsetTime + base::Milliseconds(1643235575123),
              },
              _),
          Pair(
              AttributionTriggerAndTime{
                  .trigger = AttributionTrigger(
                      /*reporting_origin=*/
                      *SuitableOrigin::Deserialize("https://b.r.test"),
                      attribution_reporting::TriggerRegistration(
                          /*filters=*/AttributionFilters(),
                          /*not_filters=*/AttributionFilters(),
                          /*debug_key=*/absl::nullopt,
                          /*aggregatable_dedup_key=*/789,
                          /*event_triggers=*/
                          attribution_reporting::EventTriggerDataList(),
                          *attribution_reporting::AggregatableTriggerDataList::
                              Create({*attribution_reporting::
                                          AggregatableTriggerData::Create(
                                              absl::MakeUint128(/*high=*/0,
                                                                /*low=*/1),
                                              /*source_keys=*/{"a"},
                                              /*filters=*/AttributionFilters(),
                                              /*not_filters=*/
                                              AttributionFilters())}),
                          /*aggregatable_values=*/
                          *attribution_reporting::AggregatableValues::Create(
                              {{"a", 1}}),
                          /*debug_reporting=*/true,
                          ::aggregation_service::mojom::AggregationCoordinator::
                              kDefault),
                      /*destination_origin=*/
                      *SuitableOrigin::Deserialize("https://a.d2.test"),
                      /*is_within_fenced_frame=*/false),
                  .time = kOffsetTime + base::Milliseconds(1643235574123),
              },
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidSourceAndTriggerParses) {
  constexpr char kJson[] = R"json({
    "sources": [{
      "timestamp": "1643235573123",
      "source_type": "event",
      "reporting_origin": "https://b.r.test",
      "source_origin": "https://b.s.test",
      "Attribution-Reporting-Register-Source": {
        "source_event_id": "456",
        "destination": "https://b.d.test"
      }
    }],
    "triggers": [{
      "timestamp": "1643235575123",
      "reporting_origin": "https://b.r.test",
      "destination_origin": " https://a.d2.test",
      "Attribution-Reporting-Register-Trigger": {}
    }]
  })json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
  EXPECT_THAT(ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                              error_stream),
              Optional(SizeIs(2)));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidCookieParses) {
  // `net::CanonicalCookie::Create()` sets
  // `net::CanonicalCookie::LastUpdateDate()` to `base::Time::Now()`, so
  // override it here to make the test deterministic.
  base::subtle::ScopedTimeClockOverrides time_override(
      /*time_override=*/[]() { return kOffsetTime + base::Seconds(1); },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  constexpr char kJson[] = R"json({"cookies": [
    {
      "timestamp": "1643235574123",
      "url": "https://r.test/x",
      "Set-Cookie": "a=b; Secure; Max-Age=5"
    }
  ]})json";

  const base::Time expected_creation_time =
      kOffsetTime + base::Milliseconds(1643235574123);

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(Pair(
          AttributionSimulatorCookie{
              .cookie = *net::CanonicalCookie::CreateUnsafeCookieForTesting(
                  /*name=*/"a",
                  /*value=*/"b",
                  /*domain=*/"r.test",
                  /*path=*/"/",
                  /*creation=*/expected_creation_time,
                  /*expiration=*/expected_creation_time + base::Seconds(5),
                  /*last_access=*/expected_creation_time,
                  /*last_updated=*/kOffsetTime + base::Seconds(1),
                  /*secure=*/true,
                  /*httponly=*/false,
                  /*same_site=*/net::CookieSameSite::UNSPECIFIED,
                  /*priority=*/net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
                  /*same_party=*/false),
              .source_url = GURL("https://r.test/x"),
          },
          _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidDataClearParses) {
  constexpr char kJson[] = R"json({"data_clears": [
    {
      "timestamp": "1643235574123",
      "delete_begin": "1643235573123",
    },
    {
      "timestamp": "1643235574123",
      "delete_end": "1643235575123",
      "origins": [
        "https://r.test",
        "https://s.test"
      ]
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          Pair(AttributionDataClear(
                   /*time=*/kOffsetTime + base::Milliseconds(1643235574123),
                   /*delete_begin=*/kOffsetTime +
                       base::Milliseconds(1643235573123),
                   /*delete_end=*/base::Time::Max(),
                   /*origins=*/absl::nullopt),
               _),
          Pair(AttributionDataClear(
                   /*time=*/kOffsetTime + base::Milliseconds(1643235574123),
                   /*delete_begin=*/base::Time::Min(),
                   /*delete_end=*/kOffsetTime +
                       base::Milliseconds(1643235575123),
                   /*origins=*/
                   base::flat_set<url::Origin>{
                       url::Origin::Create(GURL("https://r.test")),
                       url::Origin::Create(GURL("https://s.test")),
                   }),
               _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, InvalidAggregatableTriggerDataSize) {
  const struct {
    size_t size;
    bool valid;
  } kTestCases[]{
      {attribution_reporting::kMaxAggregatableTriggerDataPerTrigger, true},
      {attribution_reporting::kMaxAggregatableTriggerDataPerTrigger + 1, false},
  };

  static constexpr char kError[] =
      R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataListTooLong)";

  for (const auto test_case : kTestCases) {
    base::Value::List list;
    for (size_t i = 0; i < test_case.size; ++i) {
      list.Append("");
    }
    base::Value::Dict trigger;
    trigger.Set("aggregatable_trigger_data", std::move(list));

    base::Value::Dict dict;
    dict.Set("Attribution-Reporting-Register-Trigger", std::move(trigger));
    dict.Set("timestamp", "1643235576000");
    dict.Set("destination_origin", "https://a.d1.test");
    dict.Set("reporting_origin", "https://a.r.test");

    base::Value::List triggers;
    triggers.Append(std::move(dict));

    base::Value::Dict input;
    input.Set("triggers", std::move(triggers));

    std::ostringstream error_stream;
    EXPECT_EQ(ParseAttributionSimulationInput(base::Value(std::move(input)),
                                              kOffsetTime, error_stream),
              absl::nullopt);

    if (test_case.valid) {
      EXPECT_THAT(error_stream.str(), Not(HasSubstr(kError)));
    } else {
      EXPECT_THAT(error_stream.str(), HasSubstr(kError));
    }
  }
}

TEST(AttributionSimulatorInputParserTest, InvalidEventTriggerDataSize) {
  const struct {
    size_t size;
    bool valid;
  } kTestCases[]{
      {attribution_reporting::kMaxEventTriggerData, true},
      {attribution_reporting::kMaxEventTriggerData + 1, false},
  };

  static constexpr char kError[] =
      R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kEventTriggerDataListTooLong)";

  for (const auto test_case : kTestCases) {
    base::Value::List list;
    for (size_t i = 0; i < test_case.size; ++i) {
      list.Append("");
    }
    base::Value::Dict trigger;
    trigger.Set("event_trigger_data", std::move(list));

    base::Value::Dict dict;
    dict.Set("Attribution-Reporting-Register-Trigger", std::move(trigger));
    dict.Set("timestamp", "1643235576000");
    dict.Set("destination_origin", "https://a.d1.test");
    dict.Set("reporting_origin", "https://a.r.test");

    base::Value::List triggers;
    triggers.Append(std::move(dict));

    base::Value::Dict input;
    input.Set("triggers", std::move(triggers));

    std::ostringstream error_stream;
    EXPECT_EQ(ParseAttributionSimulationInput(base::Value(std::move(input)),
                                              kOffsetTime, error_stream),
              absl::nullopt);

    if (test_case.valid) {
      EXPECT_THAT(error_stream.str(), Not(HasSubstr(kError)));
    } else {
      EXPECT_THAT(error_stream.str(), HasSubstr(kError));
    }
  }
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
  std::ostringstream error_stream;
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
          "timestamp": "1643235574000",
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
          "timestamp": "1643235574000",
          "source_type": "navigation",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "navigation",
          "source_origin": "https://a.s.test",
          "reporting_origin": "http://r.test"
        }]})json",
    },
    {
        R"(["sources"][0]["source_origin"]: must be a valid, secure origin)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "navigation",
          "reporting_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: must be present)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: must be a dictionary)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "navigation",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": ""
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kDestinationMissing)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
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
          "timestamp": "1643235574000",
          "source_type": "NAVIGATION",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kFilterDataWrongType)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
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
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kFilterDataListWrongType)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
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
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kFilterDataValueWrongType)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
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
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kAggregationKeysWrongType)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "aggregation_keys": ""
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kAggregationKeysValueWrongFormat)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "aggregation_keys": {
              "a": "0xG"
            }
          }
        }]})json",
    },
    {
        R"(["sources"][0]["Attribution-Reporting-Register-Source"]: kAggregationKeysValueWrongFormat)",
        R"json({"sources": [{
          "timestamp": "1643235574000",
          "source_type": "event",
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test",
          "Attribution-Reporting-Register-Source": {
            "source_event_id": "123",
            "destination": "https://a.d.test",
            "aggregation_keys": {
              "a": "123"
            }
          }
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
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["destination_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "Attribution-Reporting-Register-Trigger": {}
        }]})json",
    },
    {
        R"(["triggers"][0]["reporting_origin"]: must be a valid, secure origin)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {}
        }]})json",
    },
    {
        R"(["triggers"]: must be a list)",
        R"json({"triggers": ""})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: must be present)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "destination_origin": "https://a.d1.test",
          "reporting_origin": "https://a.r.test"
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: must be a dictionary)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "destination_origin": "https://a.d1.test",
          "reporting_origin": "https://a.r.test",
          "Attribution-Reporting-Register-Trigger": ""
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kFiltersWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "destination_origin": "https://a.d1.test",
          "reporting_origin": "https://a.r.test",
          "Attribution-Reporting-Register-Trigger": {
            "filters": ""
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kFiltersListWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "destination_origin": "https://a.d1.test",
          "reporting_origin": "https://a.r.test",
          "Attribution-Reporting-Register-Trigger": {
            "not_filters": {
              "a": "x"
            }
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kEventTriggerDataListWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "event_trigger_data": 1
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataListWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_trigger_data": 5
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_trigger_data": [ 5 ]
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataSourceKeysWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_trigger_data": [{
              "key_piece": "0x123",
              "source_keys": "a"
            }]
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataSourceKeysKeyWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_trigger_data": [{
              "key_piece": "0x123",
              "source_keys": [ 5 ]
            }]
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableTriggerDataKeyPieceWrongFormat)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_trigger_data": [{
              "source_keys": [ "a" ],
              "key_piece": "0xG"
            }]
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableValuesWrongType)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_values": 5
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kAggregatableValuesValueOutOfRange)",
        R"json({"triggers": [{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "aggregatable_values": {
              "a": -5
            }
          }
        }]})json",
    },
    {
        R"(["triggers"][0]["Attribution-Reporting-Register-Trigger"]: kEventTriggerDataWrongType)",
        R"json({"triggers":[{
          "timestamp": "1643235576000",
          "reporting_origin": "https://a.r.test",
          "destination_origin": " https://a.d1.test",
          "Attribution-Reporting-Register-Trigger": {
            "event_trigger_data":[true]
          }
        }]})json",
    },
    {
        R"(["cookies"][0]["timestamp"]: must be an integer number of milliseconds)",
        R"json({"cookies": [{}]})json",
    },
    {
        R"(["cookies"][0]["timestamp"]: must be an integer number of milliseconds)",
        R"json({"cookies": [{
          "timestamp": "9223372036854775"
        }]})json",
    },
    {
        R"(["cookies"][0]["url"]: must be a valid URL)",
        R"json({"cookies": [{
        "timestamp": "1643235576000"
      }]})json",
    },
    {
        R"(["cookies"][0]["url"]: must be a valid URL)",
        R"json({"cookies": [{
        "timestamp": "1643235576000",
        "url": "!!!"
      }]})json",
    },
    {
        R"(["cookies"][0]["Set-Cookie"]: must be present)",
        R"json({"cookies": [{
        "timestamp": "1643235576000",
        "url": "https://r.test"
      }]})json",
    },
    {
        R"(["cookies"][0]: invalid cookie)",
        R"json({"cookies": [{
        "timestamp": "1643235576000",
        "url": "https://r.test",
        "Set-Cookie": ""
      }]})json",
    },
    {R"(["data_clears"][0]["timestamp"]: must be an integer number of milliseconds)",
     R"json({"data_clears": [{}]})json"},
    {R"(["data_clears"][0]["delete_begin"]: must be an integer number of milliseconds)",
     R"json({"data_clears": [{
        "timestamp": "1643235576000",
        "delete_begin": ""
      }]})json"},
    {R"(["data_clears"][0]["delete_end"]: must be an integer number of milliseconds)",
     R"json({"data_clears": [{
        "timestamp": "1643235576000",
        "delete_end": ""
      }]})json"},
    {R"(["data_clears"][0]["origins"]: must be a list)",
     R"json({"data_clears": [{
        "timestamp": "1643235576000",
        "origins": ""
      }]})json"},
    {R"(["data_clears"][0]["origins"][0]: must be a string)",
     R"json({"data_clears": [{
        "timestamp": "1643235576000",
        "origins": [1]
      }]})json"}};

INSTANTIATE_TEST_SUITE_P(AttributionSimulatorInputParserInvalidInputs,
                         AttributionSimulatorInputParseErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

}  // namespace
}  // namespace content
