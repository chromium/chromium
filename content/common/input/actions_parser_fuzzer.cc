// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include "base/json/json_reader.h"
#include "base/test/fuzztest_support.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void ActionsParserFuzzer(base::Value value) {
  content::ActionsParser parser(std::move(value));
  std::ignore = parser.Parse();
}

FUZZ_TEST(ActionsParserFuzzTest, ActionsParserFuzzer)
    .WithSeeds({*base::JSONReader::Read(
                    R"JSON( [{"source": "mouse", "id": 0,
                "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"name": "pointerUp", "x": 2, "y": 3,
                             "button": 0}]}] )JSON"),
                *base::JSONReader::Read(
                    R"JSON( [{"source": "touch", "id": 1,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                            {"name": "pointerMove", "x": 30, "y": 30},
                            {"name": "pointerUp" } ]},
               {"source": "touch", "id": 2,
                "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                            {"name": "pointerMove", "x": 50, "y": 50},
                            {"name": "pointerUp" } ]}] )JSON")});
