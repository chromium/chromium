// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_parser.h"

#include <sstream>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
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
        "report_time": "1643235573123",
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
        "report_time": "1643235573123",
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
        R"(["event_level_reports"][0]["report_time"]: must be present)",
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
        R"(["aggregatable_reports"][0]["report_time"]: must be present)",
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
