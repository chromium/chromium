// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/fragment_directive.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class FragmentDirectiveTest : public testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(FragmentDirectiveTest, ParseUniqueTextDirectives) {
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  KURL url("http://example.com/#:~:text=foo&text=bar&text=foo");
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(2u, text_directives.size());

  EXPECT_EQ("foo", text_directives[0]->textStart());
  EXPECT_EQ("bar", text_directives[1]->textStart());
}

TEST_F(FragmentDirectiveTest, ParseDuplicatesOnly) {
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  KURL url("http://example.com/#:~:text=foo&text=foo");
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(1u, text_directives.size());

  EXPECT_EQ("foo", text_directives[0]->textStart());
}

TEST_F(FragmentDirectiveTest, ParseMixedDuplicates) {
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  KURL url("http://example.com/#:~:text=a&text=b&text=a&text=c");
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(3u, text_directives.size());

  EXPECT_EQ("a", text_directives[0]->textStart());
  EXPECT_EQ("b", text_directives[1]->textStart());
  EXPECT_EQ("c", text_directives[2]->textStart());
}

TEST_F(FragmentDirectiveTest, ParseEmptyAndDuplicates) {
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  KURL url("http://example.com/#:~:text=&text=foo&text=&text=foo");
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(1u, text_directives.size());

  EXPECT_EQ("foo", text_directives[0]->textStart());
}

TEST_F(FragmentDirectiveTest, ParseEmptyAndDuplicatesMixedCase) {
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  KURL url("http://example.com/#:~:text=&text=Foo&text=&text=fOo");
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(1u, text_directives.size());

  EXPECT_EQ("Foo", text_directives[0]->textStart());
}

TEST_F(FragmentDirectiveTest, ParseTextDirectivesUpToCountLimit) {
  base::HistogramTester histogram_tester;
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  // 16 is the maximum number of directives parsed from a fragment directive.
  StringBuilder url_string;
  url_string.Append("http://example.com/#:~:text=t0");
  for (int i = 1; i < 16; ++i) {
    url_string.Append("&text=t");
    url_string.AppendNumber(i);
  }
  KURL url(url_string.ToString());
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(16u, text_directives.size());

  EXPECT_EQ("t0", text_directives.front()->textStart());
  EXPECT_EQ("t15", text_directives.back()->textStart());
  histogram_tester.ExpectUniqueSample("Blink.FragmentDirective.DirectiveCount",
                                      16, 1);
}

TEST_F(FragmentDirectiveTest, ParseTextDirectivesBeyondCountLimit) {
  base::HistogramTester histogram_tester;
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  // Only the first 16 of these 20 distinct text directives may be parsed.
  StringBuilder url_string;
  url_string.Append("http://example.com/#:~:text=t0");
  for (int i = 1; i < 20; ++i) {
    url_string.Append("&text=t");
    url_string.AppendNumber(i);
  }
  KURL url(url_string.ToString());
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(16u, text_directives.size());

  EXPECT_EQ("t0", text_directives.front()->textStart());
  EXPECT_EQ("t15", text_directives.back()->textStart());
  histogram_tester.ExpectUniqueSample("Blink.FragmentDirective.DirectiveCount",
                                      16, 1);
}

TEST_F(FragmentDirectiveTest, ParseDuplicatesDoNotCountTowardLimit) {
  base::HistogramTester histogram_tester;
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  // 16 copies of the same text directive collapse into a single directive, so
  // the unique directives that follow must still be parsed.
  StringBuilder url_string;
  url_string.Append("http://example.com/#:~:text=dup");
  for (int i = 1; i < 16; ++i) {
    url_string.Append("&text=dup");
  }
  url_string.Append("&text=foo&text=bar");
  KURL url(url_string.ToString());
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(3u, text_directives.size());

  EXPECT_EQ("dup", text_directives[0]->textStart());
  EXPECT_EQ("foo", text_directives[1]->textStart());
  EXPECT_EQ("bar", text_directives[2]->textStart());
  histogram_tester.ExpectUniqueSample("Blink.FragmentDirective.DirectiveCount",
                                      3, 1);
}

TEST_F(FragmentDirectiveTest, ParseInvalidDirectivesDoNotCountTowardLimit) {
  base::HistogramTester histogram_tester;
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  // 16 distinct but invalid text directives (too many terms) are not parsed
  // into directives, so the valid directive that follows must still be
  // parsed.
  StringBuilder url_string;
  url_string.Append("http://example.com/#:~:text=x0,b,c,d,e");
  for (int i = 1; i < 16; ++i) {
    url_string.Append("&text=x");
    url_string.AppendNumber(i);
    url_string.Append(",b,c,d,e");
  }
  url_string.Append("&text=foo");
  KURL url(url_string.ToString());
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(1u, text_directives.size());

  EXPECT_EQ("foo", text_directives[0]->textStart());
  histogram_tester.ExpectUniqueSample("Blink.FragmentDirective.DirectiveCount",
                                      1, 1);
}

TEST_F(FragmentDirectiveTest, ParseTextDirectivesCountLimitDisabled) {
  ScopedScrollToTextFragmentDirectiveLimitForTest scoped_feature(false);
  base::HistogramTester histogram_tester;
  FragmentDirective* fragment_directive =
      MakeGarbageCollected<FragmentDirective>(GetDocument());

  StringBuilder url_string;
  url_string.Append("http://example.com/#:~:text=t0");
  for (int i = 1; i < 20; ++i) {
    url_string.Append("&text=t");
    url_string.AppendNumber(i);
  }
  KURL url(url_string.ToString());
  fragment_directive->ConsumeFragmentDirective(url);

  HeapVector<Member<TextDirective>> text_directives =
      fragment_directive->GetDirectives<TextDirective>();
  ASSERT_EQ(20u, text_directives.size());

  EXPECT_EQ("t0", text_directives.front()->textStart());
  EXPECT_EQ("t19", text_directives.back()->textStart());
  histogram_tester.ExpectUniqueSample("Blink.FragmentDirective.DirectiveCount",
                                      17, 1);
}

}  // namespace blink
