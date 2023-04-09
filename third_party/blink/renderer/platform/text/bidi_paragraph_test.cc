// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using testing::ElementsAre;

TEST(BidiParagraph, SetParagraphHeuristicLtr) {
  String text(u"abc");
  BidiParagraph bidi;
  bidi.SetParagraph(text, absl::nullopt);
  EXPECT_EQ(bidi.BaseDirection(), TextDirection::kLtr);
}

TEST(BidiParagraph, SetParagraphHeuristicRtl) {
  String text(u"\u05D0\u05D1\u05D2");
  BidiParagraph bidi;
  bidi.SetParagraph(text, absl::nullopt);
  EXPECT_EQ(bidi.BaseDirection(), TextDirection::kRtl);
}

TEST(BidiParagraph, GetLogicalRuns) {
  String text(u"\u05D0\u05D1\u05D2 abc \u05D3\u05D4\u05D5");
  BidiParagraph bidi;
  bidi.SetParagraph(text, TextDirection::kRtl);
  BidiParagraph::Runs runs;
  bidi.GetLogicalRuns(text, &runs);
  EXPECT_THAT(runs, ElementsAre(BidiParagraph::Run(0, 4, 1),
                                BidiParagraph::Run(4, 7, 2),
                                BidiParagraph::Run(7, 11, 1)));
}

}  // namespace blink
