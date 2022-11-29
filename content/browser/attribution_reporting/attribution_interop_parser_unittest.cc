// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <sstream>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/attribution_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

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

using ::testing::HasSubstr;
using ::testing::Optional;

TEST(AttributionInteropParserTest, ValidInput) {
  constexpr char kInputJson[] = R"json({"input": {
      "sources": [{
        "timestamp": "1643235573123",
        "registration_request": {
          "attribution_src_url": "https://r.example/path",
          "source_origin": "https://s.example",
          "source_type": "navigation"
        },
        "responses": [{
          "url": "https://r.example/path",
          "response": {
            "Attribution-Reporting-Register-Source": {
              "destination": "https://d.test",
              "source_event_id": "123"
            }
          }
        }]
      }],
      "triggers": [{
        "timestamp": "1643235574123",
        "registration_request": {
          "attribution_src_url": "https://r.example/path",
          "destination_origin": "https://d.example"
        },
        "responses": [{
          "url": "https://r.example/path",
          "response": {
            "Attribution-Reporting-Register-Event-Trigger": [{
              "trigger_data": "7"
            }]
          }
        }]
      }]
    }})json";

  constexpr char kOutputJson[] = R"json({
      "sources": [{
        "timestamp": "1643235573123",
        "source_origin": "https://s.example",
        "source_type": "navigation",
        "reporting_origin": "https://r.example",
        "Attribution-Reporting-Register-Source": {
          "destination": "https://d.test",
          "source_event_id": "123"
        }
      }],
      "triggers": [{
        "timestamp": "1643235574123",
        "destination_origin": "https://d.example",
        "reporting_origin": "https://r.example",
        "Attribution-Reporting-Register-Event-Trigger": [{
          "trigger_data": "7"
        }]
      }]
    })json";

  base::Value input = base::test::ParseJson(kInputJson);

  std::ostringstream error_stream;
  EXPECT_THAT(AttributionInteropParser(error_stream)
                  .SimulatorInputFromInteropInput(input.GetDict()),
              Optional(base::test::IsJson(base::test::ParseJson(kOutputJson))));
  EXPECT_EQ(error_stream.str(), "");
}

TEST(AttributionInteropParserTest, ValidOutput) {
  constexpr char kInputJson[] = R"json({
      "event_level_reports": [{
        "intended_report_time": "1643235573120",
        "report_time": "1643235573123",
        "report_url": "https://r.example/path",
        "report": {
          "attribution_destination": "https://d.test",
          "randomized_trigger_rate": 0.0024,
          "source_event_id": "123",
          "source_type": "navigation",
          "trigger_data": "7"
        }
      }],
      "aggregatable_reports": [{
        "intended_report_time": "1643235573120",
        "report_time": "1643235573123",
        "report_url": "https://r.example/path",
        "report": {
          "attribution_destination": "https://d.test",
        },
        "test_info": {
          "histograms": [{
            "key": "key",
            "value": "0x159"
          }]
        }
      }]
    })json";

  constexpr char kOutputJson[] = R"json({
      "event_level_results": [{
        "report_time": "1643235573120",
        "report_url": "https://r.example/path",
        "payload": {
          "attribution_destination": "https://d.test",
          "randomized_trigger_rate": 0.0024,
          "source_event_id": "123",
          "source_type": "navigation",
          "trigger_data": "7"
        }
      }],
      "aggregatable_results": [{
        "report_time": "1643235573120",
        "report_url": "https://r.example/path",
        "payload": {
          "attribution_destination": "https://d.test",
          "histograms": [{
            "key": "key",
            "value": "0x159"
          }]
        }
      }]
    })json";

  base::Value input = base::test::ParseJson(kInputJson);

  std::ostringstream error_stream;
  EXPECT_THAT(AttributionInteropParser(error_stream)
                  .InteropOutputFromSimulatorOutput(std::move(input)),
              Optional(base::test::IsJson(base::test::ParseJson(kOutputJson))));
  EXPECT_EQ(error_stream.str(), "");
}

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
    base::Value json = base::test::ParseJson(test_case.json);
    AttributionConfig config;
    std::ostringstream error_stream;
    EXPECT_TRUE(AttributionInteropParser(error_stream)
                    .ParseConfig(json, config, test_case.required));
    EXPECT_EQ(config, test_case.expected) << json;
    EXPECT_EQ(error_stream.str(), "") << json;
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
    AttributionConfig config;
    std::ostringstream error_stream;
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(base::Value::Dict()), config,
                                  /*required=*/true));
    for (const char* field : kFields) {
      EXPECT_THAT(
          error_stream.str(),
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a positive integer formatted as base-10 string"})))
          << field;
    }
  }

  {
    AttributionConfig config;
    std::ostringstream error_stream;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "0");
    }
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(std::move(dict)), config,
                                  /*required=*/false));
    for (const char* field : kFields) {
      EXPECT_THAT(
          error_stream.str(),
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
    AttributionConfig config;
    std::ostringstream error_stream;
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(base::Value::Dict()), config,
                                  /*required=*/true));
    for (const char* field : kFields) {
      EXPECT_THAT(error_stream.str(),
                  HasSubstr(base::StrCat({"[\"", field,
                                          "\"]: must be a non-negative integer "
                                          "formatted as base-10 string"})))
          << field;
    }
  }

  {
    AttributionConfig config;
    std::ostringstream error_stream;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "-10");
    }
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(std::move(dict)), config,
                                  /*required=*/false));
    for (const char* field : kFields) {
      EXPECT_THAT(error_stream.str(),
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
    AttributionConfig config;
    std::ostringstream error_stream;
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(base::Value::Dict()), config,
                                  /*required=*/true));
    for (const char* field : kFields) {
      EXPECT_THAT(
          error_stream.str(),
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a double between 0 and 1 formatted as string"})))
          << field;
    }
  }

  {
    AttributionConfig config;
    std::ostringstream error_stream;
    base::Value::Dict dict;
    for (const char* field : kFields) {
      dict.Set(field, "1.5");
    }
    EXPECT_FALSE(AttributionInteropParser(error_stream)
                     .ParseConfig(base::Value(std::move(dict)), config,
                                  /*required=*/false));
    for (const char* field : kFields) {
      EXPECT_THAT(
          error_stream.str(),
          HasSubstr(base::StrCat(
              {"[\"", field,
               "\"]: must be a double between 0 and 1 formatted as string"})))
          << field;
    }
  }
}

struct ParseErrorTestCase {
  const char* expected_failure_substr;
  const char* json;
};

class AttributionInteropParseInputErrorTest
    : public testing::TestWithParam<ParseErrorTestCase> {};

TEST_P(AttributionInteropParseInputErrorTest, InvalidInputFails) {
  const ParseErrorTestCase& test_case = GetParam();

  base::Value value = base::test::ParseJson(test_case.json);

  std::ostringstream error_stream;

  EXPECT_EQ(AttributionInteropParser(error_stream)
                .SimulatorInputFromInteropInput(value.GetDict()),
            absl::nullopt);

  EXPECT_THAT(error_stream.str(), HasSubstr(test_case.expected_failure_substr));
}

const ParseErrorTestCase kParseInputErrorTestCases[] = {
    {
        R"(["input"]: must be present)",
        R"json({})json",
    },
    {
        R"(["input"]: must be a dictionary)",
        R"json({"input": ""})json",
    },
    {
        R"(["input"]["sources"]: must be present)",
        R"json({"input": {}})json",
    },
    {
        R"(["input"]["sources"]: must be a list)",
        R"json({"input": {"sources": ""}})json",
    },
    {
        R"(["input"]["sources"][0]: must be a dictionary)",
        R"json({"input": {"sources": [""]}})json",
    },
    {
        R"(["input"]["sources"][0]["timestamp"]: must be present)",
        R"json({"input": {"sources": [{}]}})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]: must be present)",
        R"json({"input": {"sources": [{}]}})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]: must be a dictionary)",
        R"json({"input": {
          "sources": [{
            "registration_request": ""
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]["attribution_src_url"]: must be present)",
        R"json({"input": {
          "sources": [{
            "registration_request": {}
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]["attribution_src_url"]: must be a string)",
        R"json({"input": {
          "sources": [{
            "registration_request": {
              "attribution_src_url": 1
            }
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]["timestamp"]: must not be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "timestamp": ""
            }
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["registration_request"]["reporting_origin"]: must not be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "reporting_origin": ""
            }
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"]: must be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            }
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"]: must be a list)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": ""
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"]: must have size 1)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{}, {}]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]: must be a dictionary)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [""]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["url"]: must be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{}]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["url"]: must be a string)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{
              "url": 1
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["url"]: must match https://r.test)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{
              "url": "https://d.test"
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["response"]: must be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{}]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["response"]: must be a dictionary)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{
              "response": ""
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["response"]["timestamp"]: must not be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{
              "response": {
                "timestamp": "123"
              }
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["response"]["reporting_origin"]: must not be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test"
            },
            "responses": [{
              "response": {
                "reporting_origin": ""
              }
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["sources"][0]["responses"][0]["response"]["source_type"]: must not be present)",
        R"json({"input": {
          "sources": [{
            "timestamp": "123",
            "registration_request": {
              "attribution_src_url": "https://r.test",
              "source_type": "event"
            },
            "responses": [{
              "response": {
                "source_type": ""
              }
            }]
          }]
        }})json",
    },
    {
        R"(["input"]["triggers"]: must be present)",
        R"json({"input": {}})json",
    },
};

INSTANTIATE_TEST_SUITE_P(AttributionInteropParserInvalidInputs,
                         AttributionInteropParseInputErrorTest,
                         ::testing::ValuesIn(kParseInputErrorTestCases));

class AttributionInteropParseOutputErrorTest
    : public testing::TestWithParam<ParseErrorTestCase> {};

TEST_P(AttributionInteropParseOutputErrorTest, InvalidOutputFails) {
  const ParseErrorTestCase& test_case = GetParam();

  base::Value value = base::test::ParseJson(test_case.json);

  std::ostringstream error_stream;
  AttributionInteropParser parser(error_stream);

  parser.InteropOutputFromSimulatorOutput(std::move(value));

  EXPECT_THAT(error_stream.str(), HasSubstr(test_case.expected_failure_substr));
}

const ParseErrorTestCase kParseOutputErrorTestCases[] = {
    {
        R"(input root: must be a dictionary)",
        R"json(1)json",
    },
    {
        R"(["event_level_reports"]: must be a list)",
        R"json({
          "event_level_reports": ""
        })json",
    },
    {
        R"(["event_level_reports"][0]: must be a dictionary)",
        R"json({
          "event_level_reports": [""]
        })json",
    },
    {
        R"(["event_level_reports"][0]["report"]: must be present)",
        R"json({
          "event_level_reports": [{}]
        })json",
    },
    {
        R"(["event_level_reports"][0]["report_url"]: must be present)",
        R"json({
          "event_level_reports": [{}]
        })json",
    },
    {
        R"(["event_level_reports"][0]["intended_report_time"]: must be present)",
        R"json({
          "event_level_reports": [{}]
        })json",
    },
    {
        R"(["aggregatable_reports"]: must be a list)",
        R"json({
          "aggregatable_reports": ""
        })json",
    },
    {
        R"(["aggregatable_reports"][0]: must be a dictionary)",
        R"json({
          "aggregatable_reports": [""]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["report_url"]: must be present)",
        R"json({
          "aggregatable_reports": [{}]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["intended_report_time"]: must be present)",
        R"json({
          "aggregatable_reports": [{}]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["test_info"]: must be present)",
        R"json({
          "aggregatable_reports": [{}]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["test_info"]: must be a dictionary)",
        R"json({
          "aggregatable_reports": [{
            "test_info": ""
          }]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["report"]: must be present)",
        R"json({
          "aggregatable_reports": [{
            "test_info": {}
          }]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["report"]: must be a dictionary)",
        R"json({
          "aggregatable_reports": [{
            "test_info": {},
            "report": ""
          }]
        })json",
    },
    {
        R"(["aggregatable_reports"][0]["report"]["histograms"]: must not be present)",
        R"json({
          "aggregatable_reports": [{
            "report": {
              "histograms": ""
            },
            "test_info": {
              "histograms": []
            }
          }]
        })json",
    }};

INSTANTIATE_TEST_SUITE_P(AttributionInteropParserInvalidOutputs,
                         AttributionInteropParseOutputErrorTest,
                         ::testing::ValuesIn(kParseOutputErrorTestCases));

}  // namespace
}  // namespace content
