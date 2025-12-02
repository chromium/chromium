// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class SoftNavigationPaintAttributionTrackerTest : public RenderingTest {
 public:
  SoftNavigationPaintAttributionTrackerTest() = default;
  ~SoftNavigationPaintAttributionTrackerTest() override = default;

  SoftNavigationContext* CreateSoftNavigationContext() {
    return MakeGarbageCollected<SoftNavigationContext>(
        *GetDocument().domWindow());
  }

  SoftNavigationPaintAttributionTracker* Tracker() { return tracker_.Get(); }

  Element* GetElement(const char* name) {
    return GetDocument().getElementById(AtomicString(name));
  }

 private:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
    tracker_ = GetDocument()
                   .domWindow()
                   ->GetSoftNavigationHeuristics()
                   ->GetPaintAttributionTracker();
  }

  Persistent<SoftNavigationPaintAttributionTracker> tracker_;
};

TEST_F(SoftNavigationPaintAttributionTrackerTest, ImageDirect) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <img id='target' height=50 width=50></img>
    </div>
  )HTML");

  Node* target_node = GetElement("target");
  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, ImageIndirect) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='target'>
        <img id='content' height=50 width=50></img>
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(GetElement("target"), context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(GetElement("content"), context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <!doctype html>
    <style>
      div#content {
        height: 500px;
        width: 100px;
        background-image: url("data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
        background-size: contain;
        background-repeat: no-repeat;
      }
    </style>
    <div id='ancestor'>
      <div id='target'>
        <div id='content'></div>
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(GetElement("target"), context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(GetElement("content"), context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, TextDirect) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='target'>
        Content.
      </div>
    </div>
  )HTML");

  Node* target_node = GetElement("target");
  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, TextIndirect) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='target'>
        <div id='content'>
          Content
        </div>
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(GetElement("target"), context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(GetElement("content"), context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, TextAggregation) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='target'>
        <div id='content'>
          This is some <span id='inline'>inline</span> content.
        </div>
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(GetElement("target"), context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(GetElement("content"), context));
  // This is a bit of an implementation detail, but we try to keep the minimum
  // set of nodes needed for attribution to avoid memory bloat, so we we want to
  // make sure we're we're limiting the set of elements we record to text
  // aggregating nodes and images.
  EXPECT_FALSE(Tracker()->IsAttributable(GetElement("inline"), context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       AppendInlineTextNoPreviousContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='content'>
          This is some content.
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  UpdateAllLifecyclePhasesForTest();

  // Simulate appending in JS with an active context.
  Element* content_node = GetElement("content");
  auto* element =
      MakeGarbageCollected<HTMLElement>(html_names::kBTag, GetDocument());
  element->setInnerText(" And this is bold text.");
  content_node->appendChild(element);
  Tracker()->MarkNodeAsDirectlyModified(element, context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(element, context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       AppendInlineTextOverwritePreviousAggregationNodeContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='content'>
          This is some content.
      </div>
    </div>
  )HTML");

  auto* old_context = CreateSoftNavigationContext();
  UpdateAllLifecyclePhasesForTest();

  Element* content_node = GetElement("content");
  Tracker()->MarkNodeAsDirectlyModified(content_node, old_context);
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, old_context));

  auto* new_context = CreateSoftNavigationContext();
  auto* element =
      MakeGarbageCollected<HTMLElement>(html_names::kBTag, GetDocument());
  element->setInnerText(" And this is bold text.");
  content_node->appendChild(element);
  Tracker()->MarkNodeAsDirectlyModified(element, new_context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, new_context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       AppendInlineTextInheritPreviousContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='target'>
        <div id='content'></div>
      </div>
    </div>
  )HTML");

  auto* context = CreateSoftNavigationContext();
  UpdateAllLifecyclePhasesForTest();

  Element* content_node = GetElement("content");
  Element* target_node = GetElement("target");
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);
  // This shouldn't be attributable yet since it has no text children.
  EXPECT_FALSE(Tracker()->IsAttributable(content_node, context));

  // TODO(crbug.com/423670827): For now, appending to an attributed node without
  // a `SoftNavigationContext` inherits the node's context. Evaluate if we need
  // to make changes here.
  auto* element =
      MakeGarbageCollected<HTMLElement>(html_names::kBTag, GetDocument());
  element->setInnerText(" And this is bold text.");
  content_node->appendChild(element);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, BodyAsTextAggregationNode) {
  SetBodyInnerHTML(R"HTML(
    <b id="target">Bold content</b>
  )HTML");

  Node* target_node = GetElement("target");
  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  UpdateAllLifecyclePhasesForTest();
  // This is tracked even though it's inline since it was directly modified.
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  // This text aggregates up to <body>, so that should be tracked.
  EXPECT_TRUE(Tracker()->IsAttributable(GetDocument().body(), context));
  // But, <body> should not be considered a new root for propagation.
  EXPECT_TRUE(GetDocument()
                  .body()
                  ->GetLayoutObject()
                  ->ShouldInheritSoftNavigationContext());

  // Ensure a subsequent direct modification to <body> cause it to be
  // propagated.
  Tracker()->MarkNodeAsDirectlyModified(GetDocument().body(), context);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument()
                   .body()
                   ->GetLayoutObject()
                   ->ShouldInheritSoftNavigationContext());
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       IntermediateNodesAreNotTracked) {
  SetBodyInnerHTML(R"HTML(
    <div id='outer'>
      <div id='target'>
        <div id='inner-1'>
          <div id='inner-2'>
            <div id='content'>
              Content
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  Node* target_node = GetElement("target");
  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  UpdateAllLifecyclePhasesForTest();
  // This is tracked even though it's inline since it was directly modified.
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  EXPECT_FALSE(Tracker()->IsAttributable(GetElement("inner-1"), context));
  EXPECT_FALSE(Tracker()->IsAttributable(GetElement("inner-2"), context));
  EXPECT_TRUE(Tracker()->IsAttributable(GetElement("content"), context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest, MultipleContexts) {
  SetBodyInnerHTML(R"HTML(
    <div id='outer-1'>
      <div id='target-1'>
        <div id='inner-1'>
          <div id='content-1'>
            Content
          </div>
        </div>
      </div>
    </div>
    <div id='outer-2'>
      <div id='target-2'>
        <div id='inner-2'>
          <div id='content-2'>
            Content
          </div>
        </div>
      </div>
    </div>
  )HTML");

  Node* target1_node = GetElement("target-1");
  Node* target2_node = GetElement("target-2");
  Node* content1_node = GetElement("content-1");
  Node* content2_node = GetElement("content-2");

  auto* context1 = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target1_node, context1);
  auto* context2 = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target2_node, context2);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target1_node, context1));
  EXPECT_TRUE(Tracker()->IsAttributable(content1_node, context1));
  EXPECT_TRUE(Tracker()->IsAttributable(target2_node, context2));
  EXPECT_TRUE(Tracker()->IsAttributable(content2_node, context2));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       MostRecentModificationWinsNewerContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='outer'>
      <div id='target'>
        <div id='inner'>
          <div id='content'>
            Content
          </div>
        </div>
        <div id='inner-sibling'>
          <div id='sibling-content'>
            Sibling content
          </div>
        </div>
      </div>
    </div>
  )HTML");

  Node* target_node = GetElement("target");
  Node* inner_node = GetElement("inner");
  Node* content_node = GetElement("content");
  Node* sibling_content_node = GetElement("sibling-content");
  auto* context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(sibling_content_node, context));

  auto* new_context = CreateSoftNavigationContext();
  Tracker()->MarkNodeAsDirectlyModified(inner_node, new_context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(inner_node, new_context));
  EXPECT_FALSE(Tracker()->IsAttributable(content_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(sibling_content_node, context));
  EXPECT_FALSE(Tracker()->IsAttributable(sibling_content_node, new_context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       MostRecentModificationWinsOlderContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='outer'>
      <div id='target'>
        <div id='inner'>
          <div id='content'>
            Content
          </div>
        </div>
        <div id='inner-sibling'>
          <div id='sibling-content'>
            Sibling content
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* old_context = CreateSoftNavigationContext();
  auto* new_context = CreateSoftNavigationContext();

  Node* target_node = GetElement("target");
  Node* inner_node = GetElement("inner");
  Node* content_node = GetElement("content");
  Node* sibling_content_node = GetElement("sibling-content");
  Tracker()->MarkNodeAsDirectlyModified(target_node, new_context);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, new_context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(sibling_content_node, new_context));

  // Modify `target_node` again with a context from an older interaction. This
  // should propagated all the way down.
  Tracker()->MarkNodeAsDirectlyModified(target_node, old_context);
  EXPECT_FALSE(Tracker()->IsAttributable(target_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, old_context));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, old_context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, old_context));
  EXPECT_TRUE(Tracker()->IsAttributable(sibling_content_node, old_context));

  // `inner_node` isn't being tracked, so modifying it directly will cause it to
  // be attributed to the `new_context`.
  Tracker()->MarkNodeAsDirectlyModified(inner_node, new_context);
  EXPECT_TRUE(Tracker()->IsAttributable(inner_node, new_context));
  // And pre-paint should update the relevant aggregation node.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(inner_node, new_context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, new_context));
}

TEST_F(SoftNavigationPaintAttributionTrackerTest,
       PrePaintPrunesRedundantNodes) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'>
      <div id='inner1'>
        <div id='inner2'>
          <div id='content'>
            Text
          </div>
        </div>
      </dif>
    </div>>
  )HTML");
  auto* context = CreateSoftNavigationContext();

  Node* target_node = GetElement("target");
  Node* inner_node_1 = GetElement("inner1");
  Node* inner_node_2 = GetElement("inner2");
  Node* content_node = GetElement("content");
  Tracker()->MarkNodeAsDirectlyModified(content_node, context);
  Tracker()->MarkNodeAsDirectlyModified(inner_node_2, context);
  Tracker()->MarkNodeAsDirectlyModified(inner_node_1, context);
  Tracker()->MarkNodeAsDirectlyModified(target_node, context);

  // Initially, everything is tracked.
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  EXPECT_TRUE(Tracker()->IsAttributable(inner_node_1, context));
  EXPECT_TRUE(Tracker()->IsAttributable(inner_node_2, context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, context));

  // After pre-paint, only the common container node (target) and text
  // aggregation node will be tracked.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Tracker()->IsAttributable(target_node, context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node_1, context));
  EXPECT_FALSE(Tracker()->IsAttributable(inner_node_2, context));
  EXPECT_TRUE(Tracker()->IsAttributable(content_node, context));
}

}  // namespace blink
