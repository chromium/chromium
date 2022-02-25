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
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
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
        "expiry": "864000001"
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
                   .SetSourceType(CommonSourceInfo::SourceType::kNavigation)
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
                   .SetSourceType(CommonSourceInfo::SourceType::kEvent)
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
                  .SetSourceType(CommonSourceInfo::SourceType::kEvent)
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
                  .Build(),
              _))));
  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"triggers": [
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
                  .trigger =
                      TriggerBuilder()
                          .SetReportingOrigin(
                              url::Origin::Create(GURL("https://a.r.test")))
                          .SetConversionDestination(net::SchemefulSite(
                              url::Origin::Create(GURL("https://a.d1.test"))))
                          .SetTriggerData(10)
                          .SetEventSourceTriggerData(3)
                          .SetPriority(-5)
                          .SetDedupKey(123)
                          .SetDebugKey(14)
                          .Build(),
                  .time = kOffsetTime + base::Seconds(1643235576),
              },
              _),
          Pair(
              AttributionTriggerAndTime{
                  .trigger =
                      TriggerBuilder()
                          .SetReportingOrigin(
                              url::Origin::Create(GURL("https://b.r.test")))
                          .SetConversionDestination(net::SchemefulSite(
                              url::Origin::Create(GURL("https://a.d2.test"))))
                          .SetTriggerData(0)             // default
                          .SetEventSourceTriggerData(0)  // default
                          .SetPriority(0)                // default
                          .SetDedupKey(absl::nullopt)    // default
                          .SetDebugKey(absl::nullopt)    // default
                          .Build(),
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
        "sources[0]: source_type",
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
        "sources[0]: source_time",
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
        "sources[0]: reporting_origin",
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
        "sources[0]: source_origin",
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
        "sources[0]: registration_config",
        R"json({"sources": [{
          "source_type": "navigation",
          "source_time": 1643235574,
          "reporting_origin": "https://a.r.test",
          "source_origin": "https://a.s.test"
        }]})json",
    },
    {
        "sources[0]: source_event_id",
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
        "sources[0]: destination",
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
        "sources[0]: source_type",
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
        "sources[0]: expiry",
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
        "sources[0]: priority",
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
        "sources[0]: source_event_id",
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
        "sources: must be a list",
        R"json({"sources": ""})json",
    },
    {
        "triggers[0]: registration_config",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
        }]})json",
    },
    {
        "triggers[0]: trigger_time",
        R"json({"triggers": [{
          "reporting_origin": "https://a.r.test",
          "destination": " https://a.d1.test",
          "registration_config": {}
        }]})json",
    },
    {
        "triggers[0]: destination",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "reporting_origin": "https://a.r.test",
          "registration_config": {}
        }]})json",
    },
    {
        "triggers[0]: reporting_origin",
        R"json({"triggers": [{
          "trigger_time": 1643235576,
          "destination": " https://a.d1.test",
          "registration_config": {}
        }]})json",
    },
    {
        "triggers: must be a list",
        R"json({"triggers": ""})json",
    },
};

INSTANTIATE_TEST_SUITE_P(AttributionSimulatorInputParserInvalidInputs,
                         AttributionSimulatorInputParseErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

}  // namespace
}  // namespace content
