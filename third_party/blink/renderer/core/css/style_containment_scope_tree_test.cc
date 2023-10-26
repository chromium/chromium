// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/counters_scope_tree.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class StyleContainmentScopeTreeTest : public RenderingTest {
 public:
  void CreateCounterNodeForLayoutObject(const char* id) {
    StyleContainmentScopeTree& tree =
        GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
    LayoutObject* object = GetLayoutObjectByElementId(id);
    Element* element = To<Element>(object->GetNode());
    if (AtomicString(id).Contains("use")) {
      object = object->SlowFirstChild()->SlowFirstChild();
      element = To<Element>(object->Parent()->GetNode());
    }
    StyleContainmentScope* scope =
        tree.FindOrCreateEnclosingScopeForElement(*element);
    scope->CreateCounterNodesForLayoutObject(*object);
    tree.UpdateOutermostCountersDirtyScope(scope);
  }

  void AttachLayoutCounter(const char* id) {
    StyleContainmentScopeTree& tree =
        GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
    LayoutObject* object = GetLayoutObjectByElementId(id);
    LayoutObject* it = object->NextInPreOrder();
    for (; it && !it->IsCounter(); it = it->NextInPreOrder(object)) {
    }
    LayoutCounter* layout_counter = DynamicTo<LayoutCounter>(it);
    Element& element = To<Element>(*layout_counter->Parent()->GetNode());
    ASSERT_TRUE(layout_counter);
    StyleContainmentScope* scope =
        tree.FindOrCreateEnclosingScopeForElement(element);
    scope->CreateCounterNodeForLayoutCounter(*layout_counter);
    tree.UpdateOutermostCountersDirtyScope(scope);
  }

  CountersScope* GetCountersScopeById(const char* id, const char* identifier) {
    StyleContainmentScopeTree& tree =
        GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
    LayoutObject* object = GetLayoutObjectByElementId(id);
    Element* element = To<Element>(object->GetNode());
    if (AtomicString(id).Contains("use")) {
      object = object->SlowFirstChild()->SlowFirstChild();
      element = To<Element>(object->Parent()->GetNode());
    }
    StyleContainmentScope* style_scope =
        tree.FindOrCreateEnclosingScopeForElement(*element);
    CountersScope* scope = style_scope->FindCountersScopeForElement(
        *element, AtomicString(identifier));
    return scope;
  }
};

TEST_F(StyleContainmentScopeTreeTest, ManualInsertion) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    #counter-set { counter-set: counter 1; }
    .counter-increment { counter-increment: counter 2; }
    #counter-use::before { content: counter(counter); }
    </style>
    <div id="counter-set"></div>
    <div id="counter-increment-1" class="counter-increment"></div>
    <div id="counter-increment-2" class="counter-increment"></div>
    <div id="counter-use"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  CreateCounterNodeForLayoutObject("counter-set");
  CreateCounterNodeForLayoutObject("counter-increment-1");
  CreateCounterNodeForLayoutObject("counter-increment-2");
  AttachLayoutCounter("counter-use");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use", "counter");

  EXPECT_EQ(scope->Counters().size(), 4u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 5);
}

TEST_F(StyleContainmentScopeTreeTest, ManualInsertionStyleContainment) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    #counter-set { counter-reset: counter 1; contain: style; }
    .counter-increment { counter-increment: counter 2; }
    .counter-use::before { content: counter(counter); }
    </style>
    <div id="counter-set">
      <div id="counter-use-1" class="counter-use"></div>
      <div id="counter-increment-1" class="counter-increment"></div>
      <div id="counter-increment-2" class="counter-increment"></div>
    </div>
    <div id="counter-use-2" class="counter-use"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  CreateCounterNodeForLayoutObject("counter-increment-1");
  CreateCounterNodeForLayoutObject("counter-increment-2");
  AttachLayoutCounter("counter-use-1");
  AttachLayoutCounter("counter-use-2");
  CreateCounterNodeForLayoutObject("counter-set");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-2", "counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 1);

  scope = GetCountersScopeById("counter-increment-1", "counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  // The value after `counter-increment-2` is 4 as #counter-set has no effect
  // in the element's subtree as per:
  // https://drafts.csswg.org/css-contain-2/#example-6932a400
  EXPECT_EQ(scope->Counters().front()->ValueAfter(), 2);
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 4);
  // But the use counter should access the 1 from #counter-set.
  scope = GetCountersScopeById("counter-use-1", "counter");
  EXPECT_EQ(scope->Counters().front()->ValueBefore(), 1);
}

TEST_F(StyleContainmentScopeTreeTest, ManualInsertionStyleContainmentComplex) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    #counter-reset { counter-reset: counter 1; contain: style; }
    .counter-set { counter-set: counter 1; }
    .counter-increment { counter-increment: counter 2; }
    .counter-use::before { content: counter(counter); }
    </style>
    <div id="counter-reset">
      <div id="counter-use-1" class="counter-use"></div>
      <div id="counter-increment-1" class="counter-increment"></div>
      <div id="counter-set" class="counter-set"></div>
      <div id="counter-increment-2" class="counter-increment"></div>
    </div>
    <div id="counter-use-2" class="counter-use"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  // Add counters in wrong order to see that the result scope tree is invariant
  // to order in which the elements are added.
  AttachLayoutCounter("counter-use-2");
  AttachLayoutCounter("counter-use-1");
  CreateCounterNodeForLayoutObject("counter-increment-2");
  CreateCounterNodeForLayoutObject("counter-increment-1");
  CreateCounterNodeForLayoutObject("counter-reset");
  CreateCounterNodeForLayoutObject("counter-set");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-2", "counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 1);

  scope = GetCountersScopeById("counter-increment-1", "counter");
  EXPECT_EQ(scope->Counters().size(), 3u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 1);
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 3);
  EXPECT_EQ(scope->Counters().front()->ValueAfter(), 2);
}

TEST_F(StyleContainmentScopeTreeTest,
       ManualInsertionMultipleStyleContainments) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    .counter-reset { counter-reset: counter 1; contain: style; }
    .counter-increment { counter-increment: counter 2; }
    .counter-use::before { content: counter(counter); }
    .contain-style { contain: style; }
    </style>
    <div id="counter-reset-1" class="counter-reset">
      <div id="counter-use-1" class="counter-use"></div>
      <div id="counter-increment-1" class="counter-increment"></div>
      <div id="counter-use-2" class="counter-use"></div>
      <div id="counter-reset-2" class="counter-reset">
        <div id="counter-increment-2" class="counter-increment"></div>
        <div id="counter-use-3" class="counter-use"></div>
        <div id="contain-style-1" class="contain-style">
          <div id="contain-style-2" class="contain-style">
            <div id="counter-use-4" class="counter-use"></div>
            <div id="counter-increment-3" class="counter-increment"></div>
            <div id="counter-increment-4" class="counter-increment"></div>
          </div>
        </div>
        <div id="counter-increment-5" class="counter-increment"></div>
      </div>
      <div id="counter-increment-6" class="counter-increment"></div>
    </div>
    <div id="counter-use-5" class="counter-use"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  AttachLayoutCounter("counter-use-1");
  AttachLayoutCounter("counter-use-2");
  CreateCounterNodeForLayoutObject("counter-increment-1");
  CreateCounterNodeForLayoutObject("counter-increment-2");
  CreateCounterNodeForLayoutObject("counter-increment-3");
  AttachLayoutCounter("counter-use-3");
  AttachLayoutCounter("counter-use-4");
  CreateCounterNodeForLayoutObject("counter-increment-4");
  CreateCounterNodeForLayoutObject("counter-increment-5");
  CreateCounterNodeForLayoutObject("counter-increment-6");
  AttachLayoutCounter("counter-use-5");
  CreateCounterNodeForLayoutObject("counter-reset-1");
  CreateCounterNodeForLayoutObject("counter-reset-2");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-2", "counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 2);

  scope = GetCountersScopeById("counter-use-4", "counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  // counter-use-4 is 2 as counter-increment-2 creates a new counter and does
  // +2.
  EXPECT_EQ(scope->Counters().front()->ValueBefore(), 2);

  scope = GetCountersScopeById("counter-increment-5", "counter");
  EXPECT_EQ(scope->Counters().size(), 3u);
  // counter-increment-2.
  EXPECT_EQ(scope->Counters().front()->ValueBefore(), 1);
  EXPECT_EQ(scope->Counters().front()->ValueAfter(), 2);
  // counter-increment-5.
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 4);

  scope = GetCountersScopeById("counter-increment-6", "counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  // counter-increment-6.
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 3);
}

TEST_F(StyleContainmentScopeTreeTest,
       ManualInsertionMultipleStyleContainmentsMultipleCounterNames) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    .counter-reset { counter-reset: counter 1; contain: style; }
    .my-counter-reset { counter-reset: my-counter 1; contain: style; }
    .counter-increment { counter-increment: counter 2; }
    .my-counter-increment { counter-increment: my-counter 2; }
    .counter-use::before { content: counter(counter); }
    .my-counter-use::before { content: counter(my-counter); }
    .contain-style { contain: style; }
    </style>
    <div id="counter-reset-1" class="counter-reset">
      <div id="counter-use-1" class="my-counter-use"></div>
      <div id="counter-increment-1" class="counter-increment"></div>
      <div id="counter-use-2" class="counter-use"></div>
      <div id="counter-reset-2" class="my-counter-reset">
        <div id="counter-increment-2" class="my-counter-increment"></div>
        <div id="counter-use-3" class="counter-use"></div>
        <div id="contain-style-1" class="contain-style">
          <div id="contain-style-2" class="contain-style">
            <div id="counter-use-4" class="counter-use"></div>
            <div id="counter-increment-3" class="my-counter-increment"></div>
            <div id="counter-increment-4" class="counter-increment"></div>
          </div>
        </div>
        <div id="counter-increment-5" class="counter-increment"></div>
      </div>
      <div id="counter-increment-6" class="my-counter-increment"></div>
    </div>
    <div id="counter-use-5" class="my-counter-use"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  // Add counters in wrong order to see that the result scope tree is invariant
  // to order in which the elements are added.
  AttachLayoutCounter("counter-use-5");
  CreateCounterNodeForLayoutObject("counter-increment-4");
  CreateCounterNodeForLayoutObject("counter-increment-5");
  AttachLayoutCounter("counter-use-2");
  CreateCounterNodeForLayoutObject("counter-increment-6");
  CreateCounterNodeForLayoutObject("counter-increment-1");
  CreateCounterNodeForLayoutObject("counter-increment-2");
  AttachLayoutCounter("counter-use-4");
  CreateCounterNodeForLayoutObject("counter-increment-3");
  CreateCounterNodeForLayoutObject("counter-reset-1");
  AttachLayoutCounter("counter-use-3");
  CreateCounterNodeForLayoutObject("counter-reset-2");
  AttachLayoutCounter("counter-use-1");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-5", "counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 1);
  scope = GetCountersScopeById("counter-use-5", "my-counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 0);

  scope = GetCountersScopeById("counter-use-4", "counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  // counter-use-4 is 2 as counter-increment-1 creates a new counter and does
  // +2.
  EXPECT_EQ(scope->Counters().front()->ValueBefore(), 2);

  scope = GetCountersScopeById("counter-increment-6", "my-counter");
  EXPECT_EQ(scope->Counters().size(), 2u);
  // counter-increment-6.
  EXPECT_EQ(scope->Counters().back()->ValueAfter(), 3);
}

TEST_F(StyleContainmentScopeTreeTest, ManualInsertionSelfStyleContainment) {
  // Tests the bug: https://crbug.com/1462328.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .list-item {
        contain: style;
        counter-increment: counter;
      }
      .list-item::before {
        content: '[' counter(counter, decimal) ']';
      }
    </style>
    <div id="A1" class="list-item">
      <div id="B1" class="list-item"></div>
      <div id="B2" class="list-item"></div>
      <div id="B3" class="list-item"></div>
    </div>
    <div id="A2" class="list-item"></div>
    <div id="A3" class="list-item"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  for (const AtomicString& letter : {AtomicString("A"), AtomicString("B")}) {
    for (int i = 1; i <= 3; ++i) {
      AtomicString id = letter + char(i + '0');
      CreateCounterNodeForLayoutObject(id.Ascii().c_str());
      AttachLayoutCounter(id.Ascii().c_str());
    }
  }

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  for (const AtomicString& letter : {AtomicString("A"), AtomicString("B")}) {
    for (int i = 1; i <= 3; ++i) {
      AtomicString id = letter + char(i + '0');
      CountersScope* scope =
          GetCountersScopeById(id.Ascii().c_str(), "counter");

      for (CounterNode* node : scope->Counters()) {
        if (auto* node_element = DynamicTo<Element>(node->Owner().GetNode())) {
          if (node_element->GetIdAttribute() == AtomicString(id)) {
            EXPECT_EQ(node->ValueAfter(), i);
          }
        }
      }
      EXPECT_EQ(scope->Counters().size(), 3u);
    }
  }
}

TEST_F(StyleContainmentScopeTreeTest, ManualInsertionSanityCheck) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .cnt::before {
        content: counter(counter);
      }
    </style>
    <div id="counter-reset-1" style="counter-reset: counter 2; contain: style;">
      <div>
        <div id="counter-reset-2" style="counter-reset: counter 5;">
          <div id="counter-use-1" class="cnt"></div>
          <div id="counter-use-2" class="cnt"></div>
        </div>
      </div>
      <div>
        <div id="counter-use-3" class="cnt"></div>
        <div id="counter-use-4" class="cnt"></div>
      </div>
    </div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  AttachLayoutCounter("counter-use-1");
  AttachLayoutCounter("counter-use-2");
  AttachLayoutCounter("counter-use-3");
  AttachLayoutCounter("counter-use-4");
  CreateCounterNodeForLayoutObject("counter-reset-1");
  CreateCounterNodeForLayoutObject("counter-reset-2");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-1", "counter");
  EXPECT_EQ(scope->Counters().size(), 3u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 5);

  scope = GetCountersScopeById("counter-use-3", "counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 2);
}

TEST_F(StyleContainmentScopeTreeTest,
       ManualInsertionSanityCheckStyleContainment) {
  // This one was failing with the previous Chrome implementation.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .cnt::before {
        content: counter(counter);
      }
    </style>
    <div id="counter-reset-1" style="counter-reset: counter 5;">
      <div id="counter-use-1" class="cnt"></div>
      <div id="counter-use-2" class="cnt"></div>
    </div>
    <div style="contain: style;">
      <div id="counter-use-3" class="cnt"></div>
      <div id="counter-use-4" class="cnt"></div>
    </div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  AttachLayoutCounter("counter-use-1");
  AttachLayoutCounter("counter-use-2");
  AttachLayoutCounter("counter-use-3");
  AttachLayoutCounter("counter-use-4");
  CreateCounterNodeForLayoutObject("counter-reset-1");

  StyleContainmentScopeTree& tree =
      GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
  tree.UpdateCounters();

  CountersScope* scope = GetCountersScopeById("counter-use-1", "counter");
  EXPECT_EQ(scope->Counters().size(), 3u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 5);

  scope = GetCountersScopeById("counter-use-3", "counter");
  EXPECT_EQ(scope->Counters().size(), 1u);
  EXPECT_EQ(scope->Counters().back()->ValueBefore(), 5);
}

}  // namespace blink
