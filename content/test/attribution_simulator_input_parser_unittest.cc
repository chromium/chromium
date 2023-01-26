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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
         a.delete_end == b.delete_end && a.origins == b.origins &&
         a.delete_rate_limit_data == b.delete_rate_limit_data;
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

  return out << ",delete_rate_limit_data=" << c.delete_rate_limit_data << "}";
}

namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;

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

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;

  auto result = ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                                error_stream);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 2u);

  const auto* source1 = absl::get_if<StorableSource>(&result->front());
  ASSERT_TRUE(source1);

  const auto* source2 = absl::get_if<StorableSource>(&result->back());
  ASSERT_TRUE(source2);

  EXPECT_EQ(source1->common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235574123));
  EXPECT_EQ(source1->common_info().source_type(),
            AttributionSourceType::kNavigation);
  EXPECT_EQ(source1->common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://a.r.test"));
  EXPECT_EQ(source1->common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://a.s.test"));
  EXPECT_EQ(source1->common_info().destination_origin(),
            *SuitableOrigin::Deserialize("https://a.d.test"));
  EXPECT_FALSE(source1->is_within_fenced_frame());

  EXPECT_EQ(source2->common_info().source_time(),
            kOffsetTime + base::Milliseconds(1643235573123));
  EXPECT_EQ(source2->common_info().source_type(),
            AttributionSourceType::kEvent);
  EXPECT_EQ(source2->common_info().reporting_origin(),
            *SuitableOrigin::Deserialize("https://b.r.test"));
  EXPECT_EQ(source2->common_info().source_origin(),
            *SuitableOrigin::Deserialize("https://b.s.test"));
  EXPECT_EQ(source2->common_info().destination_origin(),
            *SuitableOrigin::Deserialize("https://b.d.test"));
  EXPECT_FALSE(source2->is_within_fenced_frame());

  EXPECT_THAT(error_stream.str(), IsEmpty());
}

TEST(AttributionSimulatorInputParserTest, ValidTriggerParses) {
  constexpr char kJson[] = R"json({"triggers": [
    {
      "timestamp": "1643235575123",
      "reporting_origin": "https://a.r.test",
      "destination_origin": " https://b.d.test",
      "Attribution-Reporting-Register-Trigger": {}
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;

  auto result = ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                                error_stream);

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
  EXPECT_THAT(ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                              error_stream),
              Optional(ElementsAre(AttributionSimulatorCookie{
                  .cookie = *net::CanonicalCookie::CreateUnsafeCookieForTesting(
                      /*name=*/"a",
                      /*value=*/"b",
                      /*domain=*/"r.test",
                      /*path=*/"/",
                      /*creation=*/expected_creation_time,
                      /*expiration=*/expected_creation_time + base::Seconds(5),
                      /*last_access=*/expected_creation_time,
                      /*last_update=*/kOffsetTime + base::Seconds(1),
                      /*secure=*/true,
                      /*httponly=*/false,
                      /*same_site=*/net::CookieSameSite::UNSPECIFIED,
                      /*priority=*/net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
                      /*same_party=*/false),
                  .source_url = GURL("https://r.test/x"),
              })));
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
      ],
      "delete_rate_limit_data": false
    }
  ]})json";

  base::Value value = base::test::ParseJson(kJson);
  std::ostringstream error_stream;
  EXPECT_THAT(
      ParseAttributionSimulationInput(std::move(value), kOffsetTime,
                                      error_stream),
      Optional(ElementsAre(
          AttributionDataClear(
              /*time=*/kOffsetTime + base::Milliseconds(1643235574123),
              /*delete_begin=*/kOffsetTime + base::Milliseconds(1643235573123),
              /*delete_end=*/base::Time::Max(),
              /*origins=*/absl::nullopt,
              /*delete_rate_limit_data=*/true),
          AttributionDataClear(
              /*time=*/kOffsetTime + base::Milliseconds(1643235574123),
              /*delete_begin=*/base::Time::Min(),
              /*delete_end=*/kOffsetTime + base::Milliseconds(1643235575123),
              /*origins=*/
              base::flat_set<url::Origin>{
                  url::Origin::Create(GURL("https://r.test")),
                  url::Origin::Create(GURL("https://s.test")),
              },
              /*delete_rate_limit_data=*/false))));
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
