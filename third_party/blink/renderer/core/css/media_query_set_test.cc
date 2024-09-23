// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

typedef struct {
  const char* input;
  const char* output;
} MediaQuerySetTestCase;

// If `unknown_substitute` is non-null, then any unknown queries are
// substituted with that string.
static void TestMediaQuery(const char* input,
                           const char* output,
                           MediaQuerySet& query_set,
                           String unknown_substitute = String()) {
  StringBuilder actual;
  wtf_size_t j = 0;
  while (j < query_set.QueryVector().size()) {
    const MediaQuery& query = *query_set.QueryVector()[j];
    if (!unknown_substitute.IsNull() && query.HasUnknown()) {
      actual.Append(unknown_substitute);
    } else {
      actual.Append(query.CssText());
    }
    ++j;
    if (j >= query_set.QueryVector().size()) {
      break;
    }
    actual.Append(", ");
  }
  if (output) {
    ASSERT_EQ(String(output), actual.ToString());
  } else {
    ASSERT_EQ(String(input), actual.ToString());
  }
}

TEST(MediaQuerySetTest, Basic) {
  // The first string represents the input string.
  // The second string represents the output string, if present.
  // Otherwise, the output string is identical to the first string.
  MediaQuerySetTestCase test_cases[] = {
      {"", nullptr},
      {" ", ""},
      {"screen", nullptr},
      {"screen and (color)", nullptr},
      {"all and (min-width:500px)", "(min-width: 500px)"},
      {"all and (min-width:/*bla*/500px)", "(min-width: 500px)"},
      {"(min-width:500px)", "(min-width: 500px)"},
      {"screen and (color), projection and (color)", nullptr},
      {"not screen and (color)", nullptr},
      {"only screen and (color)", nullptr},
      {"screen and (color), projection and (color)", nullptr},
      {"aural and (device-aspect-ratio: 16 / 9)", nullptr},
      {"speech and (min-device-width: 800px)", nullptr},
      {"example", nullptr},
      {"screen and (max-weight: 3kg) and (color), (monochrome)",
       "not all, (monochrome)"},
      {"(min-width: -100px)", "(min-width: -100px)"},
      {"(width:100gil)", "not all"},
      {"(example, all,), speech", "not all, speech"},
      {"&test, screen", "not all, screen"},
      {"print and (min-width: 25cm)", nullptr},
      {"screen and (min-width: 400px) and (max-width: 700px)", nullptr},
      {"screen and (device-width: 800px)", nullptr},
      {"screen and (device-height: 60em)", nullptr},
      {"screen and (device-height: 60rem)", nullptr},
      {"screen and (device-height: 60ch)", nullptr},
      {"screen and (device-aspect-ratio: 16 / 9)", nullptr},
      {"(device-aspect-ratio: 16.1/9.0)", "(device-aspect-ratio: 16.1 / 9)"},
      {"(device-aspect-ratio: 16.0)", "(device-aspect-ratio: 16 / 1)"},
      {"(device-aspect-ratio: 16/ 9)", "(device-aspect-ratio: 16 / 9)"},
      {"(device-aspect-ratio: 16/\r9)", "(device-aspect-ratio: 16 / 9)"},
      {"all and (color)", "(color)"},
      {"all and (min-color: 1)", "(min-color: 1)"},
      {"all and (min-color: 1.0)", "not all"},
      {"all and (min-color: 2)", "(min-color: 2)"},
      {"all and (color-index)", "(color-index)"},
      {"all and (min-color-index: 1)", "(min-color-index: 1)"},
      {"all and (monochrome)", "(monochrome)"},
      {"all and (min-monochrome: 1)", "(min-monochrome: 1)"},
      {"all and (min-monochrome: 2)", "(min-monochrome: 2)"},
      {"print and (monochrome)", nullptr},
      {"handheld and (grid) and (max-width: 15em)", nullptr},
      {"handheld and (grid) and (max-device-height: 7em)", nullptr},
      {"screen and (max-width: 50%)", "not all"},
      {"screen and (max-WIDTH: 500px)", "screen and (max-width: 500px)"},
      {"screen and (max-width: 24.4em)", nullptr},
      {"screen and (max-width: 24.4EM)", "screen and (max-width: 24.4em)"},
      {"screen and (max-width: blabla)", "not all"},
      {"screen and (max-width: 1)", "not all"},
      {"screen and (max-width: 0)", "screen and (max-width: 0)"},
      {"screen and (max-width: 1deg)", "not all"},
      {"handheld and (min-width: 20em), \nscreen and (min-width: 20em)",
       "handheld and (min-width: 20em), screen and (min-width: 20em)"},
      {"print and (min-resolution: 300dpi)", nullptr},
      {"print and (min-resolution: 118dpcm)", nullptr},
      {"(resolution: 0.83333333333333333333dppx)",
       "(resolution: 0.833333dppx)"},
      {"(resolution: 2.4dppx)", nullptr},
      {"(resolution: calc(1dppx))", "(resolution: calc(1dppx))"},
      {"(resolution: calc(1x))", "(resolution: calc(1dppx))"},
      {"(resolution: calc(96dpi))", "(resolution: calc(1dppx))"},
      {"(resolution: calc(1x + 2x))", "(resolution: calc(3dppx))"},
      {"(resolution: calc(3x - 2x))", "(resolution: calc(1dppx))"},
      {"(resolution: calc(1x * 3))", "(resolution: calc(3dppx))"},
      {"(resolution: calc(6x / 2))", "(resolution: calc(3dppx))"},
      {"all and(color)", "not all"},
      {"all and (", "not all"},
      {"test;,all", "not all, all"},
      {"(color:20example)", "not all"},
      {"not braille", nullptr},
      {",screen", "not all, screen"},
      {",all", "not all, all"},
      {",,all,,", "not all, not all, all, not all, not all"},
      {",,all,, ", "not all, not all, all, not all, not all"},
      {",screen,,&invalid,,",
       "not all, screen, not all, not all, not all, not all"},
      {",screen,,(invalid,),,",
       "not all, screen, not all, not all, not all, not all"},
      {",(all,),,", "not all, not all, not all, not all"},
      {",", "not all, not all"},
      {"  ", ""},
      {"(color", "(color)"},
      {"(min-color: 2", "(min-color: 2)"},
      {"(orientation: portrait)", nullptr},
      {"tv and (scan: progressive)", nullptr},
      {"(pointer: coarse)", nullptr},
      {"(min-orientation:portrait)", "not all"},
      {"all and (orientation:portrait)", "(orientation: portrait)"},
      {"all and (orientation:landscape)", "(orientation: landscape)"},
      {"NOT braille, tv AND (max-width: 200px) and (min-WIDTH: 100px) and "
       "(orientation: landscape), (color)",
       "not braille, tv and (max-width: 200px) and (min-width: 100px) and "
       "(orientation: landscape), (color)"},
      {"(m\\61x-width: 300px)", "(max-width: 300px)"},
      {"(max-width: 400\\70\\78)", "(max-width: 400px)"},
      {"(max-width: 500\\0070\\0078)", "(max-width: 500px)"},
      {"(max-width: 600\\000070\\000078)", "(max-width: 600px)"},
      {"(max-width: 700px), (max-width: 700px)",
       "(max-width: 700px), (max-width: 700px)"},
      {"(max-width: 800px()), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px(()), (max-width: 900px)", "not all"},
      {"(max-width: 600px(())))), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px(((((((((())))), (max-width: 500px)", "not all"},
      {"(max-width: 800px[]), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px[[]), (max-width: 900px)", "not all"},
      {"(max-width: 600px[[]]]]), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px[[[[[[[[[[]]]]), (max-width: 500px)", "not all"},
      {"(max-width: 800px{}), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px{{}), (max-width: 900px)", "not all"},
      {"(max-width: 600px{{}}}}), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px{{{{{{{{{{}}}}), (max-width: 500px)", "not all"},
      {"[(), (max-width: 400px)", "not all"},
      {"[{}, (max-width: 500px)", "not all"},
      {"[{]}], (max-width: 900px)", "not all, (max-width: 900px)"},
      {"[{[]{}{{{}}}}], (max-width: 900px)", "not all, (max-width: 900px)"},
      {"[{[}], (max-width: 900px)", "not all"},
      {"[({)}], (max-width: 900px)", "not all"},
      {"[]((), (max-width: 900px)", "not all"},
      {"((), (max-width: 900px)", "not all"},
      {"(foo(), (max-width: 900px)", "not all"},
      {"[](()), (max-width: 900px)", "not all, (max-width: 900px)"},
      {"all an[isdfs bla())()]icalc(i)(()), (max-width: 400px)",
       "not all, (max-width: 400px)"},
      {"all an[isdfs bla())(]icalc(i)(()), (max-width: 500px)", "not all"},
      {"all an[isdfs bla())(]icalc(i)(())), (max-width: 600px)", "not all"},
      {"all an[isdfs bla())(]icalc(i)(()))], (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: '40px')", "not all"},
      {"('max-width': 40px)", "not all"},
      {"'\"'\", (max-width: 900px)", "not all"},
      {"'\"\"\"', (max-width: 900px)", "not all, (max-width: 900px)"},
      {"\"'\"', (max-width: 900px)", "not all"},
      {"\"'''\", (max-width: 900px)", "not all, (max-width: 900px)"},
      {"not not", "not all"},
      {"not and", "not all"},
      {"not only", "not all"},
      {"not or", "not all"},
      {"only not", "not all"},
      {"only and", "not all"},
      {"only only", "not all"},
      {"only or", "not all"},
      {"layer", "not all"},
      {"not layer", "not all"},
      {"not (orientation)", nullptr},
      {"only (orientation)", "not all"},
      {"(max-width: 800px()), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px(()), (max-width: 900px)", "not all"},
      {"(max-width: 600px(())))), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px(((((((((())))), (max-width: 500px)", "not all"},
      {"(max-width: 800px[]), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px[[]), (max-width: 900px)", "not all"},
      {"(max-width: 600px[[]]]]), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px[[[[[[[[[[]]]]), (max-width: 500px)", "not all"},
      {"(max-width: 800px{}), (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(max-width: 900px{{}), (max-width: 900px)", "not all"},
      {"(max-width: 600px{{}}}}), (max-width: 600px)",
       "not all, (max-width: 600px)"},
      {"(max-width: 500px{{{{{{{{{{}}}}), (max-width: 500px)", "not all"},
      {"[(), (max-width: 400px)", "not all"},
      {"[{}, (max-width: 500px)", "not all"},
      {"[{]}], (max-width: 900px)", "not all, (max-width: 900px)"},
      {"[{[]{}{{{}}}}], (max-width: 900px)", "not all, (max-width: 900px)"},
      {"[{[}], (max-width: 900px)", "not all"},
      {"[({)}], (max-width: 900px)", "not all"},
      {"[]((), (max-width: 900px)", "not all"},
      {"((), (max-width: 900px)", "not all"},
      {"(foo(), (max-width: 900px)", "not all"},
      {"[](()), (max-width: 900px)", "not all, (max-width: 900px)"},
      {"all an[isdfs bla())(i())]icalc(i)(()), (max-width: 400px)",
       "not all, (max-width: 400px)"},
      {"all an[isdfs bla())(]icalc(i)(()), (max-width: 500px)", "not all"},
      {"all an[isdfs bla())(]icalc(i)(())), (max-width: 600px)", "not all"},
      {"all an[isdfs bla())(]icalc(i)(()))], (max-width: 800px)",
       "not all, (max-width: 800px)"},
      {"(inline-size > 0px)", "not all"},
      {"(min-inline-size: 0px)", "not all"},
      {"(max-inline-size: 0px)", "not all"},
      {"(block-size > 0px)", "not all"},
      {"(min-block-size: 0px)", "not all"},
      {"(max-block-size: 0px)", "not all"},
      {"(device-aspect-ratio: calc(16.1)/calc(9.0))",
       "(device-aspect-ratio: calc(16.1) / calc(9))"},
      {"(device-aspect-ratio: calc(16.1)/9.0)",
       "(device-aspect-ratio: calc(16.1) / 9)"},
  };

  for (const MediaQuerySetTestCase& test : test_cases) {
    SCOPED_TRACE(String(test.input));
    // This test was originally written for mediaqueries-3, and does not
    // differentiate between real parse errors ("not all") and queries which
    // have parts which match the <general-enclosed> production.
    TestMediaQuery(test.input, test.output,
                   *MediaQuerySet::Create(test.input, nullptr), "not all");
  }
}

TEST(MediaQuerySetTest, CSSMediaQueries4) {
  MediaQuerySetTestCase test_cases[] = {
      {"(width: 100px) or (width: 200px)", nullptr},
      {"(width: 100px)or (width: 200px)", "(width: 100px) or (width: 200px)"},
      {"(width: 100px) or (width: 200px) or (color)", nullptr},
      {"screen and (width: 100px) or (width: 200px)", "not all"},
      {"(height: 100px) and (width: 100px) or (width: 200px)", "not all"},
      {"(height: 100px) or (width: 100px) and (width: 200px)", "not all"},
      {"((width: 100px))", nullptr},
      {"(((width: 100px)))", nullptr},
      {"(   (   (width: 100px) ) )", "(((width: 100px)))"},
      {"(width: 100px) or ((width: 200px) or (width: 300px))", nullptr},
      {"(width: 100px) and ((width: 200px) or (width: 300px))", nullptr},
      {"(width: 100px) or ((width: 200px) and (width: 300px))", nullptr},
      {"(width: 100px) or ((width: 200px) and (width: 300px)) and (width: "
       "400px)",
       "not all"},
      {"(width: 100px) and ((width: 200px) and (width: 300px)) or (width: "
       "400px)",
       "not all"},
      {"(width: 100px) or ((width: 200px) and (width: 300px)) or (width: "
       "400px)",
       nullptr},
      {"(width: 100px) and ((width: 200px) and (width: 300px)) and (width: "
       "400px)",
       nullptr},
      {"not (width: 100px)", nullptr},
      {"(width: 100px) and (not (width: 200px))", nullptr},
      {"(width: 100px) and not (width: 200px)", "not all"},
      {"(width < 100px)", nullptr},
      {"(width <= 100px)", nullptr},
      {"(width > 100px)", nullptr},
      {"(width >= 100px)", nullptr},
      {"(width = 100px)", nullptr},
      {"(100px < width)", nullptr},
      {"(100px <= width)", nullptr},
      {"(100px > width)", nullptr},
      {"(100px >= width)", nullptr},
      {"(100px = width)", nullptr},
      {"(100px < width < 200px)", nullptr},
      {"(100px <= width <= 200px)", nullptr},
      {"(100px < width <= 200px)", nullptr},
      {"(100px <= width < 200px)", nullptr},
      {"(200px > width > 100px)", nullptr},
      {"(200px >= width >= 100px)", nullptr},
      {"(200px > width >= 100px)", nullptr},
      {"(200px >= width > 100px)", nullptr},
      {"(not (width < 100px)) and (height > 200px)", nullptr},
      {"(width<100px)", "(width < 100px)"},
      {"(width>=100px)", "(width >= 100px)"},
      {"(width=100px)", "(width = 100px)"},
      {"(200px>=width > 100px)", "(200px >= width > 100px)"},
      {"(200px>=width>100px)", "(200px >= width > 100px)"},
  };

  for (const MediaQuerySetTestCase& test : test_cases) {
    SCOPED_TRACE(String(test.input));
    TestMediaQuery(test.input, test.output,
                   *MediaQuerySet::Create(test.input, nullptr), "<unknown>");
  }
}

// https://drafts.csswg.org/mediaqueries-4/#typedef-general-enclosed
TEST(MediaQuerySetTest, GeneralEnclosed) {
  const char* unknown_cases[] = {
      "()",
      "( )",
      "(1)",
      "( 1 )",
      "(1px)",
      "(unknown)",
      "(unknown: 50kg)",
      "unknown()",
      "unknown(1)",
      "(a b c)",
      "(width <> height)",
      "( a! b; )",
      "not screen and (unknown)",
      "not all and (unknown)",
      "not all and (width) and (unknown)",
      "not all and (not ((width) or (unknown)))",
      "(width: 100px) or (max-width: 50%)",
      "(width: 100px) or ((width: 200px) and (width: 300px) or (width: "
      "400px))",
      "(width: 100px) or ((width: 200px) or (width: 300px) and (width: "
      "400px))",
      "(width < 50%)",
      "(width < 100px nonsense)",
      "(100px nonsense < 100px)",
      "(width == 100px)",
      "(width << 100px)",
      "(width <> 100px)",
      "(100px == width)",
      "(100px < = width)",
      "(100px > = width)",
      "(100px==width)",
      "(100px , width)",
      "(100px,width)",
      "(100px ! width)",
      "(1px < width > 2px)",
      "(1px > width < 2px)",
      "(1px <= width > 2px)",
      "(1px > width <= 2px)",
      "(1px = width = 2px)",
      "(min-width < 10px)",
      "(max-width < 10px)",
      "(10px < min-width)",
      "(10px < min-width < 20px)",
      "(100px ! width < 200px)",
      "(100px < width ! 200px)",
      "(100px <)",
      "(100px < )",
      "(100px < width <)",
      "(100px < width < )",
      "(50% < width < 200px)",
      "(100px < width < 50%)",
      "(100px nonsense < width < 200px)",
      "(100px < width < 200px nonsense)",
      "(100px < width : 200px)",
  };

  for (const char* input : unknown_cases) {
    SCOPED_TRACE(String(input));
    TestMediaQuery(input, input, *MediaQuerySet::Create(input, nullptr));

    // When we parse something as <general-enclosed>, we'll serialize whatever
    // was specified, so it's not clear if we took the <general-enclosed> path
    // during parsing or not. In order to verify this, run the same test again,
    // substituting unknown queries with
    // "<unknown>".
    TestMediaQuery(input, "<unknown>", *MediaQuerySet::Create(input, nullptr),
                   "<unknown>");
  }

  const char* invalid_cases[] = {
      "(])",
      "(url(as'df))",
  };

  for (const char* input : invalid_cases) {
    SCOPED_TRACE(String(input));
    TestMediaQuery(input, "not all", *MediaQuerySet::Create(input, nullptr));
  }
}

}  // namespace blink
