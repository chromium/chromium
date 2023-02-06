// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/attribution_simulator_input_parser.h"

#include <ostream>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
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
  return a.trigger == b.trigger && a.time == b.time &&
         a.debug_permission == b.debug_permission;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTriggerAndTime& t) {
  return out << "{time=" << t.time << ",trigger=" << t.trigger
             << ",debug_permission=" << t.debug_permission << "}";
}

bool operator==(const AttributionSource& a, const AttributionSource& b) {
  return a.source == b.source && a.debug_permission == b.debug_permission;
}

std::ostream& operator<<(std::ostream& out, const AttributionSource& s) {
  return out << "{source=" << s.source
             << ",debug_permission=" << s.debug_permission << "}";
}

namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;

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
    base::Value::Dict value = base::test::ParseJsonDict(json);
    auto result =
        ParseAttributionSimulationInput(std::move(value), kOffsetTime);
    ASSERT_TRUE(result.has_value()) << json;
    EXPECT_THAT(*result, IsEmpty()) << json;
  }
}

TEST(AttributionSimulatorInputParserTest, ValidSourceParses) {
  constexpr char kJson[] = R"json({"sources": [
    {
      "timestamp": "1643235574123",
      "source_type": "navigation",
      "reporting_origin": "https://a.r.test",
      "source_origin": "https://a.s.test",
      "debug_permission": true,
      "Attribution-Reporting-Register-Source": {
        "destination": "https://a.d.test"
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
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  auto result = ParseAttributionSimulationInput(std::move(value), kOffsetTime);

  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_EQ(result->size(), 2u);

  const auto* source1 = absl::get_if<AttributionSource>(&result->front());
  ASSERT_TRUE(source1);

  const auto* source2 = absl::get_if<AttributionSource>(&result->back());
  ASSERT_TRUE(source2);

  EXPECT_EQ(source1->source.common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235574123));
  EXPECT_EQ(source1->source.common_info().source_type(),
            AttributionSourceType::kNavigation);
  EXPECT_EQ(source1->source.common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(source1->source.common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://a.s.test"));
  EXPECT_EQ(source1->source.common_info().destination_site(),
            net::SchemefulSite::Deserialize("https://d.test"));
  EXPECT_FALSE(source1->source.is_within_fenced_frame());
  EXPECT_TRUE(source1->debug_permission);

  EXPECT_EQ(source2->source.common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235573123));
  EXPECT_EQ(source2->source.common_info().source_type(),
            AttributionSourceType::kEvent);
  EXPECT_EQ(source2->source.common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://b.r.test"));
  EXPECT_EQ(source2->source.common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://b.s.test"));
  EXPECT_EQ(source2->source.common_info().destination_site(),
            net::SchemefulSite::Deserialize("https://d.test"));
  EXPECT_FALSE(source2->source.is_within_fenced_frame());
  EXPECT_FALSE(source2->debug_permission);
}

TEST(AttributionSimulatorInputParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"triggers": [
    {
      "timestamp": "1643235575123",
      "reporting_origin": "https://a.r.test",
      "destination_origin": " https://b.d.test",
      "debug_permission": true,
      "Attribution-Reporting-Register-Trigger": {}
    }
  ]})json";

  base::Value::Dict value = base::test::ParseJsonDict(kJson);

  auto result = ParseAttributionSimulationInput(std::move(value), kOffsetTime);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);

  const auto* trigger =
      absl::get_if<AttributionTriggerAndTime>(&result->front());
  ASSERT_TRUE(trigger);

  EXPECT_EQ(trigger->time, kOffsetTime + base::Milliseconds(1643235575123));
  EXPECT_EQ(trigger->trigger.reporting_origin(),
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(trigger->trigger.destination_origin(),
            *SuitableOrigin::Deserialize("https://b.d.test"));
  EXPECT_EQ(trigger->trigger.attestation(), absl::nullopt);
  EXPECT_FALSE(trigger->trigger.is_within_fenced_frame());
  EXPECT_TRUE(trigger->debug_permission);
}

struct ParseErrorTestCase {
  const char* expected_failure_substr;
  const char* json;
};

class AttributionSimulatorInputParseErrorTest
    : public testing::TestWithParam<ParseErrorTestCase> {};

TEST_P(AttributionSimulatorInputParseErrorTest, InvalidInputFails) {
  const ParseErrorTestCase& test_case = GetParam();

  base::Value::Dict value = base::test::ParseJsonDict(test_case.json);
  auto result = ParseAttributionSimulationInput(std::move(value), kOffsetTime);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr(test_case.expected_failure_substr));
}

const ParseErrorTestCase kParseErrorTestCases[] = {
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
};

INSTANTIATE_TEST_SUITE_P(AttributionSimulatorInputParserInvalidInputs,
                         AttributionSimulatorInputParseErrorTest,
                         ::testing::ValuesIn(kParseErrorTestCases));

}  // namespace
}  // namespace content
