// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(SvgParserUtilitiesTest, SkipOptionalSVGSpacesOrDelimiter) {
  {
    String str = ",foo";
    EXPECT_EQ(1u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }
  {
    String str = "  ,foo";
    EXPECT_EQ(3u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }
  {
    String str = ", foo";
    EXPECT_EQ(2u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }
  {
    String str = "_foo";
    EXPECT_EQ(1u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 1u));
  }
  {
    String str = "  ";
    EXPECT_EQ(2u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }
  {
    String str = ",";
    EXPECT_EQ(1u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }
  {
    String str = "";
    EXPECT_EQ(0u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u));
  }

  {
    String str = "  /foo";
    EXPECT_EQ(3u, SkipOptionalSVGSpacesOrDelimiter(str.Span8(), 0u, '/'));
  }
}

}  // namespace blink
