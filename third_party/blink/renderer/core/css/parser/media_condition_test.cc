// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

typedef struct {
  const char* input;
  const char* output;
} MediaConditionTestCase;

TEST(MediaConditionParserTest, Basic) {
  // The first string represents the input string.
  // The second string represents the output string, if present.
  // Otherwise, the output string is identical to the first string.
  MediaConditionTestCase test_cases[] = {
      {"screen", "not all"},
      {"screen and (color)", "not all"},
      {"all and (min-width:500px)", "not all"},
      {"(min-width:500px)", "(min-width: 500px)"},
      {"(min-width : -100px)", "(min-width: -100px)"},
      {"(min-width: 100px) and print", "not all"},
      {"(min-width: 100px) and (max-width: 900px)", nullptr},
      {"(min-width: [100px) and (max-width: 900px)", "not all"},
      {"not (min-width: 900px)", "not (min-width: 900px)"},
      {"not ( blabla)", "not ( blabla)"},  // <general-enclosed>
      {"", ""},
      {" ", ""},
      {",(min-width: 500px)", "not all"},
      {"(min-width: 500px),", "not all"},
      {"(width: 1px) and (width: 2px), (width: 3px)", "not all"},
      {"(width: 1px) and (width: 2px), screen", "not all"},
      {"(min-width: 500px), (min-width: 500px)", "not all"},
      {"not (min-width: 500px), not (min-width: 500px)", "not all"},
      {"(width: 1px), screen", "not all"},
      {"screen, (width: 1px)", "not all"},
      {"screen, (width: 1px), print", "not all"},

      {nullptr, nullptr}  // Do not remove the terminator line.
  };

  for (unsigned i = 0; test_cases[i].input; ++i) {
    SCOPED_TRACE(test_cases[i].input);
    CSSTokenizer tokenizer(test_cases[i].input);
    const auto tokens = tokenizer.TokenizeToEOF();
    MediaQuerySet* media_condition_query_set =
        MediaQueryParser::ParseMediaCondition(CSSParserTokenRange(tokens),
                                              nullptr);
    String query_text = media_condition_query_set->MediaText();
    const char* expected_text =
        test_cases[i].output ? test_cases[i].output : test_cases[i].input;
    EXPECT_EQ(String(expected_text), query_text);
  }
}

}  // namespace blink
