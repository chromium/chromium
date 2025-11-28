// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/fragment_directive.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

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

}  // namespace blink
