// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class DoNothingListener : public NativeEventListener {
  void Invoke(ExecutionContext*, Event*) override {}
};

}  // namespace

// TODO(hayato): It's hard to see what's happening in these tests.
// It would be better to refactor these tests.
TEST(TreeScopeAdopterTest, SimpleMove) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* doc1 = Document::CreateForTest(execution_context.GetExecutionContext());
  auto* doc2 = Document::CreateForTest(execution_context.GetExecutionContext());

  Element* html1 = doc1->CreateRawElement(html_names::kHTMLTag);
  doc1->AppendChild(html1);
  Element* div1 = doc1->CreateRawElement(html_names::kDivTag);
  html1->AppendChild(div1);

  Element* html2 = doc2->CreateRawElement(html_names::kHTMLTag);
  doc2->AppendChild(html2);
  Element* div2 = doc1->CreateRawElement(html_names::kDivTag);
  html2->AppendChild(div2);

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter1(*div1, *doc1);
  EXPECT_FALSE(adopter1.NeedsScopeChange());

  TreeScopeAdopter adopter2(*div2, *doc1);
  ASSERT_TRUE(adopter2.NeedsScopeChange());

  adopter2.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
}

TEST(TreeScopeAdopterTest, MoveNestedShadowRoots) {
  test::TaskEnvironment task_environment;
  DummyPageHolder source_page_holder;
  auto* source_doc = &source_page_holder.GetDocument();
  NativeEventListener* listener = MakeGarbageCollected<DoNothingListener>();

  Element* html = source_doc->CreateRawElement(html_names::kHTMLTag);
  source_doc->body()->AppendChild(html);
  Element* outer_div = source_doc->CreateRawElement(html_names::kDivTag);
  html->AppendChild(outer_div);

  ShadowRoot& outer_shadow =
      outer_div->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  Element* middle_div = source_doc->CreateRawElement(html_names::kDivTag);
  outer_shadow.AppendChild(middle_div);

  // Append an event target to a node that will be traversed after the inner
  // shadow tree.
  Element* middle_target = source_doc->CreateRawElement(html_names::kDivTag);
  outer_shadow.AppendChild(middle_target);
  ASSERT_TRUE(middle_target->addEventListener(event_type_names::kMousewheel,
                                              listener, false));

  ShadowRoot& middle_shadow =
      middle_div->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  Element* inner_div = source_doc->CreateRawElement(html_names::kDivTag);
  middle_shadow.AppendChild(inner_div);
  // This event listener may force a consistency check in EventHandlerRegistry,
  // which will check the consistency of the above event handler as a
  // side-effect too.
  ASSERT_TRUE(inner_div->addEventListener(event_type_names::kMousewheel,
                                          listener, false));

  DummyPageHolder target_page_holder;
  auto* target_doc = &target_page_holder.GetDocument();
  ASSERT_TRUE(target_doc->GetPage());
  ASSERT_NE(source_doc->GetPage(), target_doc->GetPage());

  TreeScopeAdopter adopter(*outer_div, *target_doc);
  ASSERT_TRUE(adopter.NeedsScopeChange());

  adopter.Execute();
  EXPECT_EQ(outer_shadow.ownerDocument(), target_doc);
  EXPECT_EQ(middle_shadow.ownerDocument(), target_doc);
}

}  // namespace blink
