// Copyright 2014 The Chromium Authors. All rights reserved.
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

static void TestMediaQuery(MediaQuerySetTestCase test,
                           MediaQuerySet& query_set) {
  StringBuilder output;
  wtf_size_t j = 0;
  while (j < query_set.QueryVector().size()) {
    String query_text = query_set.QueryVector()[j]->CssText();
    output.Append(query_text);
    ++j;
    if (j >= query_set.QueryVector().size())
      break;
    output.Append(", ");
  }
  if (test.output)
    ASSERT_EQ(test.output, output.ToString());
  else
    ASSERT_EQ(test.input, output.ToString());
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
      {"aural and (device-aspect-ratio: 16/9)", nullptr},
      {"speech and (min-device-width: 800px)", nullptr},
      {"example", nullptr},
      {"screen and (max-weight: 3kg) and (color), (monochrome)",
       "not all, (monochrome)"},
      {"(min-width: -100px)", "not all"},
      {"(example, all,), speech", "not all, speech"},
      {"&test, screen", "not all, screen"},
      {"print and (min-width: 25cm)", nullptr},
      {"screen and (min-width: 400px) and (max-width: 700px)",
       "screen and (max-width: 700px) and (min-width: 400px)"},
      {"screen and (device-width: 800px)", nullptr},
      {"screen and (device-height: 60em)", nullptr},
      {"screen and (device-height: 60rem)", nullptr},
      {"screen and (device-height: 60ch)", nullptr},
      {"screen and (device-aspect-ratio: 16/9)", nullptr},
      {"(device-aspect-ratio: 16.0/9.0)", "not all"},
      {"(device-aspect-ratio: 16/ 9)", "(device-aspect-ratio: 16/9)"},
      {"(device-aspect-ratio: 16/\r9)", "(device-aspect-ratio: 16/9)"},
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
      {"screen and (max-width: 0)", nullptr},
      {"screen and (max-width: 1deg)", "not all"},
      {"handheld and (min-width: 20em), \nscreen and (min-width: 20em)",
       "handheld and (min-width: 20em), screen and (min-width: 20em)"},
      {"print and (min-resolution: 300dpi)", nullptr},
      {"print and (min-resolution: 118dpcm)", nullptr},
      {"(resolution: 0.83333333333333333333dppx)",
       "(resolution: 0.833333333333333dppx)"},
      {"(resolution: 2.4dppx)", nullptr},
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
      {"not (orientation)", "not all"},
      {"only (orientation)", "not all"},
      {nullptr, nullptr}  // Do not remove the terminator line.
  };

  for (unsigned i = 0; test_cases[i].input; ++i) {
    scoped_refptr<MediaQuerySet> query_set =
        MediaQuerySet::Create(test_cases[i].input);
    TestMediaQuery(test_cases[i], *query_set);
  }
}

}  // namespace blink
