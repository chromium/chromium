// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/request_headers_internal.h"

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "net/http/structured_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {
using GreaseContext =
    ::network::AttributionReportingHeaderGreaseOptions::GreaseContext;

using ::network::mojom::AttributionSupport;
}  // namespace

bool operator==(const AttributionReportingHeaderGreaseOptions& a,
                const AttributionReportingHeaderGreaseOptions& b) {
  const auto tie = [](const AttributionReportingHeaderGreaseOptions& o) {
    return std::make_tuple(o.reverse, o.swap_greases, o.context1, o.context2,
                           o.use_front1, o.use_front2);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out, GreaseContext context) {
  switch (context) {
    case GreaseContext::kNone:
      return out << "kNone";
    case GreaseContext::kKey:
      return out << "kKey";
    case GreaseContext::kValue:
      return out << "kValue";
    case GreaseContext::kParamName:
      return out << "kParamName";
  }
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReportingHeaderGreaseOptions& o) {
  return out << o.reverse << ", " << o.swap_greases << ", " << o.context1
             << ", " << o.context2 << ", " << o.use_front1 << ", "
             << o.use_front2;
}

namespace {

using ::network::mojom::AttributionReportingEligibility;

TEST(AttributionRequestHeadersTest, GreaseOptionsFromBits) {
  const struct {
    uint8_t bits;
    AttributionReportingHeaderGreaseOptions expected;
  } kTestCases[] = {
      {
          0b00000000,
          {},
      },
      {
          0b00000001,
          {.reverse = true},
      },
      {
          0b00000010,
          {.swap_greases = true},
      },
      {
          0b00000100,
          {.context1 = GreaseContext::kKey},
      },
      {
          0b00001000,
          {.context1 = GreaseContext::kValue},
      },
      {
          0b00001100,
          {.context1 = GreaseContext::kParamName},
      },
      {
          0b00010000,
          {.context2 = GreaseContext::kKey},
      },
      {
          0b00100000,
          {.context2 = GreaseContext::kValue},
      },
      {
          0b00110000,
          {.context2 = GreaseContext::kParamName},
      },
      {
          0b01000000,
          {.use_front1 = true},
      },
      {
          0b10000000,
          {.use_front2 = true},
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        test_case.expected,
        AttributionReportingHeaderGreaseOptions::FromBits(test_case.bits));
  }
}

TEST(AttributionRequestHeadersTest, NoGrease) {
  const struct {
    AttributionReportingEligibility eligibility;
    const char* expected;
  } kTestCases[] = {
      {AttributionReportingEligibility::kEmpty, ""},
      {AttributionReportingEligibility::kEventSource, "event-source"},
      {AttributionReportingEligibility::kNavigationSource, "navigation-source"},
      {AttributionReportingEligibility::kTrigger, "trigger"},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       "event-source, trigger"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        test_case.expected,
        SerializeAttributionReportingEligibleHeader(
            test_case.eligibility, AttributionReportingHeaderGreaseOptions()));
  }
}

TEST(AttributionRequestHeadersTest, Greases) {
  const struct {
    AttributionReportingEligibility eligibility;
    AttributionReportingHeaderGreaseOptions options;
    const char* expected;
  } kTestCases[] = {
      // reverse with vectors of varying lengths
      {
          AttributionReportingEligibility::kEmpty,
          {.reverse = true},
          "",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.reverse = true},
          "event-source",
      },
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {.reverse = true},
          "trigger, event-source",
      },
      // grease 1
      {
          AttributionReportingEligibility::kEmpty,
          {.context1 = GreaseContext::kKey},
          "not-event-source",
      },
      {
          AttributionReportingEligibility::kEmpty,
          {.context1 = GreaseContext::kValue},
          "",
      },
      {
          AttributionReportingEligibility::kEmpty,
          {.context1 = GreaseContext::kParamName},
          "",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context1 = GreaseContext::kKey},
          "event-source, not-trigger",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context1 = GreaseContext::kValue},
          "event-source=trigger",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context1 = GreaseContext::kParamName},
          "event-source;trigger",
      },
      {
          AttributionReportingEligibility::kNavigationSource,
          {.context1 = GreaseContext::kKey},
          "navigation-source, not-event-source",
      },
      {
          AttributionReportingEligibility::kTrigger,
          {.context1 = GreaseContext::kKey},
          "trigger, not-navigation-source",
      },
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {.context1 = GreaseContext::kKey},
          "event-source, trigger, not-navigation-source",
      },
      // grease 2
      {
          AttributionReportingEligibility::kEmpty,
          {.context2 = GreaseContext::kKey},
          "not-trigger",
      },
      {
          AttributionReportingEligibility::kEmpty,
          {.context2 = GreaseContext::kValue},
          "",
      },
      {
          AttributionReportingEligibility::kEmpty,
          {.context2 = GreaseContext::kParamName},
          "",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context2 = GreaseContext::kKey},
          "event-source, not-navigation-source",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context2 = GreaseContext::kValue},
          "event-source=navigation-source",
      },
      {
          AttributionReportingEligibility::kEventSource,
          {.context2 = GreaseContext::kParamName},
          "event-source;navigation-source",
      },
      {
          AttributionReportingEligibility::kNavigationSource,
          {.context2 = GreaseContext::kKey},
          "navigation-source, not-trigger",
      },
      {
          AttributionReportingEligibility::kTrigger,
          {.context2 = GreaseContext::kKey},
          "trigger, not-event-source",
      },
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {.context2 = GreaseContext::kKey},
          "event-source, trigger",
      },
      // swap greases
      {
          AttributionReportingEligibility::kEmpty,
          {
              .swap_greases = true,
              .context1 = GreaseContext::kKey,
          },
          "not-trigger",
      },
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {
              .swap_greases = true,
              .context1 = GreaseContext::kKey,
          },
          "event-source, trigger",
      },
      // use_front
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {
              .context1 = GreaseContext::kValue,
              .use_front1 = false,
          },
          "event-source, trigger=navigation-source",
      },
      {
          AttributionReportingEligibility::kEventSourceOrTrigger,
          {
              .context1 = GreaseContext::kValue,
              .use_front1 = true,
          },
          "event-source=navigation-source, trigger",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              SerializeAttributionReportingEligibleHeader(test_case.eligibility,
                                                          test_case.options));
  }
}

TEST(AttributionRequestHeadersTest, GetAttributionSupportHeader) {
  const struct {
    mojom::AttributionSupport attribution_support;
    std::vector<std::string> required_keys;
    std::vector<std::string> prohibited_keys;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb, {"web"}, {"os"}},
      {mojom::AttributionSupport::kWebAndOs, {"os", "web"}, {}},
      {mojom::AttributionSupport::kOs, {"os"}, {"web"}},
      {mojom::AttributionSupport::kNone, {}, {"os", "web"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.attribution_support);

    std::string actual =
        GetAttributionSupportHeader(test_case.attribution_support,
                                    AttributionReportingHeaderGreaseOptions());

    auto dict = net::structured_headers::ParseDictionary(actual);
    EXPECT_TRUE(dict.has_value());

    for (const auto& key : test_case.required_keys) {
      EXPECT_TRUE(dict->contains(key)) << key;
    }

    for (const auto& key : test_case.prohibited_keys) {
      EXPECT_FALSE(dict->contains(key)) << key;
    }
  }
}

TEST(AttributionRequestHeadersTest, Greases_Support) {
  const struct {
    AttributionSupport support;
    AttributionReportingHeaderGreaseOptions options;
    const char* expected;
  } kTestCases[] = {
      // reverse with vectors of varying lengths
      {
          AttributionSupport::kNone,
          {.reverse = true},
          "",
      },
      {
          AttributionSupport::kWeb,
          {.reverse = true},
          "web",
      },
      {
          AttributionSupport::kWebAndOs,
          {.reverse = true},
          "web, os",
      },
      // grease 1
      {
          AttributionSupport::kNone,
          {.context1 = GreaseContext::kKey},
          "not-os",
      },
      {
          AttributionSupport::kWeb,
          {.context1 = GreaseContext::kKey},
          "web, not-os",
      },
      {
          AttributionSupport::kWeb,
          {.context1 = GreaseContext::kValue},
          "web=os",
      },
      {
          AttributionSupport::kWeb,
          {.context1 = GreaseContext::kParamName},
          "web;os",
      },
      {
          AttributionSupport::kOs,
          {.context1 = GreaseContext::kKey},
          "os, not-web",
      },
      {
          AttributionSupport::kOs,
          {.context1 = GreaseContext::kValue},
          "os=web",
      },
      {
          AttributionSupport::kOs,
          {.context1 = GreaseContext::kParamName},
          "os;web",
      },
      {
          AttributionSupport::kWebAndOs,
          {.context1 = GreaseContext::kKey},
          "os, web",
      },
      {
          AttributionSupport::kWebAndOs,
          {.context1 = GreaseContext::kValue},
          "os, web",
      },
      {
          AttributionSupport::kWebAndOs,
          {.context1 = GreaseContext::kParamName},
          "os, web",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected, GetAttributionSupportHeader(
                                      test_case.support, test_case.options));
  }
}

}  // namespace
}  // namespace network
