// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/svg/svg_path_string_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_string_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

bool ParsePath(const char* input, String& output) {
  String input_string(input);
  SVGPathStringSource source(input_string);
  SVGPathStringBuilder builder;
  bool had_error = svg_path_parser::ParsePath(source, builder);
  output = builder.Result();
  // Coerce a null result to empty.
  if (output.IsNull())
    output = g_empty_string;
  return had_error;
}

#define VALID(input, expected)             \
  {                                        \
    String output;                         \
    EXPECT_TRUE(ParsePath(input, output)); \
    EXPECT_EQ(expected, output);           \
  }

#define MALFORMED(input, expected)          \
  {                                         \
    String output;                          \
    EXPECT_FALSE(ParsePath(input, output)); \
    EXPECT_EQ(expected, output);            \
  }

TEST(SVGPathParserTest, Simple) {
  test::TaskEnvironment task_environment;
  VALID("M1,2", "M 1 2");
  VALID("m1,2", "m 1 2");
  VALID("M100,200 m3,4", "M 100 200 m 3 4");
  VALID("M100,200 L3,4", "M 100 200 L 3 4");
  VALID("M100,200 l3,4", "M 100 200 l 3 4");
  VALID("M100,200 H3", "M 100 200 H 3");
  VALID("M100,200 h3", "M 100 200 h 3");
  VALID("M100,200 V3", "M 100 200 V 3");
  VALID("M100,200 v3", "M 100 200 v 3");
  VALID("M100,200 Z", "M 100 200 Z");
  VALID("M100,200 z", "M 100 200 Z");
  VALID("M100,200 C3,4,5,6,7,8", "M 100 200 C 3 4 5 6 7 8");
  VALID("M100,200 c3,4,5,6,7,8", "M 100 200 c 3 4 5 6 7 8");
  VALID("M100,200 S3,4,5,6", "M 100 200 S 3 4 5 6");
  VALID("M100,200 s3,4,5,6", "M 100 200 s 3 4 5 6");
  VALID("M100,200 Q3,4,5,6", "M 100 200 Q 3 4 5 6");
  VALID("M100,200 q3,4,5,6", "M 100 200 q 3 4 5 6");
  VALID("M100,200 T3,4", "M 100 200 T 3 4");
  VALID("M100,200 t3,4", "M 100 200 t 3 4");
  VALID("M100,200 A3,4,5,0,0,6,7", "M 100 200 A 3 4 5 0 0 6 7");
  VALID("M100,200 A3,4,5,1,0,6,7", "M 100 200 A 3 4 5 1 0 6 7");
  VALID("M100,200 A3,4,5,0,1,6,7", "M 100 200 A 3 4 5 0 1 6 7");
  VALID("M100,200 A3,4,5,1,1,6,7", "M 100 200 A 3 4 5 1 1 6 7");
  VALID("M100,200 a3,4,5,0,0,6,7", "M 100 200 a 3 4 5 0 0 6 7");
  VALID("M100,200 a3,4,5,0,1,6,7", "M 100 200 a 3 4 5 0 1 6 7");
  VALID("M100,200 a3,4,5,1,0,6,7", "M 100 200 a 3 4 5 1 0 6 7");
  VALID("M100,200 a3,4,5,1,1,6,7", "M 100 200 a 3 4 5 1 1 6 7");
  VALID("M100,200 a3,4,5,006,7", "M 100 200 a 3 4 5 0 0 6 7");
  VALID("M100,200 a3,4,5,016,7", "M 100 200 a 3 4 5 0 1 6 7");
  VALID("M100,200 a3,4,5,106,7", "M 100 200 a 3 4 5 1 0 6 7");
  VALID("M100,200 a3,4,5,116,7", "M 100 200 a 3 4 5 1 1 6 7");
  MALFORMED("M100,200 a3,4,5,2,1,6,7", "M 100 200");
  MALFORMED("M100,200 a3,4,5,1,2,6,7", "M 100 200");

  VALID("M100,200 a0,4,5,0,0,10,0 a4,0,5,0,0,0,10 a0,0,5,0,0,-10,0 z",
        "M 100 200 a 0 4 5 0 0 10 0 a 4 0 5 0 0 0 10 a 0 0 5 0 0 -10 0 Z");

  VALID("M1,2,3,4", "M 1 2 L 3 4");
  VALID("m100,200,3,4", "m 100 200 l 3 4");

  VALID("M 100-200", "M 100 -200");
  VALID("M 0.6.5", "M 0.6 0.5");

  VALID(" M1,2", "M 1 2");
  VALID("  M1,2", "M 1 2");
  VALID("\tM1,2", "M 1 2");
  VALID("\nM1,2", "M 1 2");
  VALID("\rM1,2", "M 1 2");
  MALFORMED("\vM1,2", "");
  MALFORMED("xM1,2", "");
  VALID("M1,2 ", "M 1 2");
  VALID("M1,2\t", "M 1 2");
  VALID("M1,2\n", "M 1 2");
  VALID("M1,2\r", "M 1 2");
  MALFORMED("M1,2\v", "M 1 2");
  MALFORMED("M1,2x", "M 1 2");
  MALFORMED("M1,2 L40,0#90", "M 1 2 L 40 0");

  VALID("", "");
  VALID(" ", "");
  MALFORMED("x", "");
  MALFORMED("L1,2", "");
  VALID("M.1 .2 L.3 .4 .5 .6", "M 0.1 0.2 L 0.3 0.4 L 0.5 0.6");

  MALFORMED("M", "");
  MALFORMED("M\0", "");

  MALFORMED("M1,1Z0", "M 1 1 Z");
  MALFORMED("M1,1z0", "M 1 1 Z");

  VALID("M1,1h2,3", "M 1 1 h 2 h 3");
  VALID("M1,1H2,3", "M 1 1 H 2 H 3");
  VALID("M1,1v2,3", "M 1 1 v 2 v 3");
  VALID("M1,1V2,3", "M 1 1 V 2 V 3");

  MALFORMED("M1,1c2,3 4,5 6,7 8", "M 1 1 c 2 3 4 5 6 7");
  VALID("M1,1c2,3 4,5 6,7 8,9 10,11 12,13",
        "M 1 1 c 2 3 4 5 6 7 c 8 9 10 11 12 13");
  MALFORMED("M1,1C2,3 4,5 6,7 8", "M 1 1 C 2 3 4 5 6 7");
  VALID("M1,1C2,3 4,5 6,7 8,9 10,11 12,13",
        "M 1 1 C 2 3 4 5 6 7 C 8 9 10 11 12 13");
  MALFORMED("M1,1s2,3 4,5 6", "M 1 1 s 2 3 4 5");
  VALID("M1,1s2,3 4,5 6,7 8,9", "M 1 1 s 2 3 4 5 s 6 7 8 9");
  MALFORMED("M1,1S2,3 4,5 6", "M 1 1 S 2 3 4 5");
  VALID("M1,1S2,3 4,5 6,7 8,9", "M 1 1 S 2 3 4 5 S 6 7 8 9");
  MALFORMED("M1,1q2,3 4,5 6", "M 1 1 q 2 3 4 5");
  VALID("M1,1q2,3 4,5 6,7 8,9", "M 1 1 q 2 3 4 5 q 6 7 8 9");
  MALFORMED("M1,1Q2,3 4,5 6", "M 1 1 Q 2 3 4 5");
  VALID("M1,1Q2,3 4,5 6,7 8,9", "M 1 1 Q 2 3 4 5 Q 6 7 8 9");
  MALFORMED("M1,1t2,3 4", "M 1 1 t 2 3");
  VALID("M1,1t2,3 4,5", "M 1 1 t 2 3 t 4 5");
  MALFORMED("M1,1T2,3 4", "M 1 1 T 2 3");
  VALID("M1,1T2,3 4,5", "M 1 1 T 2 3 T 4 5");
  MALFORMED("M1,1a2,3,4,0,0,5,6 7", "M 1 1 a 2 3 4 0 0 5 6");
  VALID("M1,1a2,3,4,0,0,5,6 7,8,9,0,0,10,11",
        "M 1 1 a 2 3 4 0 0 5 6 a 7 8 9 0 0 10 11");
  MALFORMED("M1,1A2,3,4,0,0,5,6 7", "M 1 1 A 2 3 4 0 0 5 6");
  VALID("M1,1A2,3,4,0,0,5,6 7,8,9,0,0,10,11",
        "M 1 1 A 2 3 4 0 0 5 6 A 7 8 9 0 0 10 11");

  // Scientific notation.
  VALID("M1e2,10e1", "M 100 100");
  VALID("M100e0,100", "M 100 100");
  VALID("M1e+2,1000e-1", "M 100 100");
  VALID("M1e2.5", "M 100 0.5");
  VALID("M0.00000001e10 100", "M 100 100");
  VALID("M1e-46,50 h1e38", "M 0 50 h 1.00000e+38");
  VALID("M0,50 h1e-123456789123456789123", "M 0 50 h 0");
  MALFORMED("M0,50 h1e39", "M 0 50");
  MALFORMED("M0,50 h1e123456789123456789123", "M 0 50");
  MALFORMED("M0,50 h1e-.5", "M 0 50");
  MALFORMED("M0,50 h1e+.5", "M 0 50");
}

#undef MALFORMED
#undef VALID

SVGParsingError ParsePathWithError(const char* input) {
  String input_string(input);
  SVGPathStringSource source(input_string);
  SVGPathStringBuilder builder;
  svg_path_parser::ParsePath(source, builder);
  return source.ParseError();
}

#define EXPECT_ERROR(input, expectedLocus, expectedError) \
  {                                                       \
    SVGParsingError error = ParsePathWithError(input);    \
    EXPECT_EQ(expectedError, error.Status());             \
    EXPECT_TRUE(error.HasLocus());                        \
    EXPECT_EQ(expectedLocus, error.Locus());              \
  }

TEST(SVGPathParserTest, ErrorReporting) {
  test::TaskEnvironment task_environment;
  // Missing initial moveto.
  EXPECT_ERROR(" 10 10", 1u, SVGParseStatus::kExpectedMoveToCommand);
  EXPECT_ERROR("L 10 10", 0u, SVGParseStatus::kExpectedMoveToCommand);
  // Invalid command letter.
  EXPECT_ERROR("M 10 10 #", 8u, SVGParseStatus::kExpectedPathCommand);
  EXPECT_ERROR("M 10 10 E 100 100", 8u, SVGParseStatus::kExpectedPathCommand);
  // Invalid number.
  EXPECT_ERROR("M 10 10 L100 ", 13u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M 10 10 L100 #", 13u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M 10 10 L100#100", 12u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M0,0 A#,10 0 0,0 20,20", 6u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M0,0 A10,# 0 0,0 20,20", 9u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M0,0 A10,10 # 0,0 20,20", 12u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M0,0 A10,10 0 0,0 #,20", 18u, SVGParseStatus::kExpectedNumber);
  EXPECT_ERROR("M0,0 A10,10 0 0,0 20,#", 21u, SVGParseStatus::kExpectedNumber);
  // Invalid arc-flag.
  EXPECT_ERROR("M0,0 A10,10 0 #,0 20,20", 14u,
               SVGParseStatus::kExpectedArcFlag);
  EXPECT_ERROR("M0,0 A10,10 0 0,# 20,20", 16u,
               SVGParseStatus::kExpectedArcFlag);
  EXPECT_ERROR("M0,0 A10,10 0 0,2 20,20", 16u,
               SVGParseStatus::kExpectedArcFlag);
}

#undef EXPECT_ERROR

}  // namespace

}  // namespace blink
