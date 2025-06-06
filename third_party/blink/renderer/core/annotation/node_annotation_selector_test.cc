// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/node_annotation_selector.h"

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/annotation/annotation_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NodeAnnotationSelectorTest : public RenderingTest {
 public:
  NodeAnnotationSelectorTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override { RenderingTest::SetUp(); }

  Range* DocumentBodyRange() {
    Range* range = GetDocument().createRange();
    range->selectNode(GetDocument().body());
    return range;
  }
};

TEST_F(NodeAnnotationSelectorTest, ConstructorAndSerialize) {
  DOMNodeId test_id = 123;
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(test_id);
  EXPECT_EQ(selector->Serialize(), "123");
}

TEST_F(NodeAnnotationSelectorTest, IsTextSelectorReturnsTrue) {
  DOMNodeId test_id = 456;
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(test_id);
  EXPECT_FALSE(selector->IsTextSelector());
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeFound) {
  SetBodyInnerHTML(R"HTML(
    <div id="first_div">Hello</div>
  )HTML");
  Element* div_element =
      GetDocument().getElementById(AtomicString("first_div"));

  DOMNodeId target_node_id = div_element->GetDomNodeId();
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(target_node_id);
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();

  ASSERT_NE(range_result, nullptr);
  EXPECT_FALSE(range_result->IsNull());

  PositionInFlatTree start_position = range_result->StartPosition();
  PositionInFlatTree end_position = range_result->EndPosition();

  EXPECT_EQ(start_position.AnchorNode(), div_element);
  EXPECT_EQ(start_position.OffsetInContainerNode(), 0);
  EXPECT_EQ(end_position.AnchorNode(), div_element);
  EXPECT_EQ(end_position.AnchorType(), PositionAnchorType::kAfterChildren);
  EXPECT_EQ(PlainText(range_result->ToEphemeralRange()), "Hello");
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeFoundMultipleTextNodes) {
  SetBodyInnerHTML(R"HTML(
    Other text to make sure we are not returning the whole document.
    <div id="first_div">
        <p>Hello</p><p>World</p>
    </div>
    Other text to make sure we are not returning the whole document.
  )HTML");
  Element* div_element =
      GetDocument().getElementById(AtomicString("first_div"));

  DOMNodeId target_node_id = div_element->GetDomNodeId();
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(target_node_id);
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();

  ASSERT_NE(range_result, nullptr);
  EXPECT_FALSE(range_result->IsNull());

  PositionInFlatTree start_position = range_result->StartPosition();
  PositionInFlatTree end_position = range_result->EndPosition();

  EXPECT_EQ(start_position.AnchorNode(), div_element);
  EXPECT_EQ(start_position.OffsetInContainerNode(), 0);
  EXPECT_EQ(end_position.AnchorNode(), div_element);
  EXPECT_EQ(end_position.AnchorType(), PositionAnchorType::kAfterChildren);
  EXPECT_EQ(PlainText(range_result->ToEphemeralRange()),
            String("Hello\n\nWorld\n\n"));
}

TEST_F(NodeAnnotationSelectorTest,
       FindRangeNodeFoundMultipleTextNodesAndImageInBetween) {
  SetBodyInnerHTML(R"HTML(
    Other text to make sure we are not returning the whole document.
    <span id="first_span">
        <p>Hello</p>
        <img src="nonexistent.gif" width="42" height="42"
            style="border:5px solid black">
        <p>World</p>
    </span>
    Other text to make sure we are not returning the whole document.
  )HTML");
  Element* span_element =
      GetDocument().getElementById(AtomicString("first_span"));

  DOMNodeId target_node_id = span_element->GetDomNodeId();
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(target_node_id);
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();

  ASSERT_NE(range_result, nullptr);
  EXPECT_FALSE(range_result->IsNull());

  PositionInFlatTree start_position = range_result->StartPosition();
  PositionInFlatTree end_position = range_result->EndPosition();

  EXPECT_EQ(start_position.AnchorNode(), span_element);
  EXPECT_EQ(start_position.OffsetInContainerNode(), 0);
  EXPECT_EQ(end_position.AnchorNode(), span_element);
  EXPECT_EQ(end_position.AnchorType(), PositionAnchorType::kAfterChildren);
  EXPECT_EQ(PlainText(range_result->ToEphemeralRange()),
            String("Hello\n\n\nWorld\n\n"));
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeFoundButNoText) {
  SetBodyInnerHTML(R"HTML(
    Other text to make sure we are not returning the whole document.
    Also add white space and new lines only in the div to make sure
    we're not selecting only empty space.
    <div id="first_div">
      </div>
    Other text to make sure we are not returning the whole document.
  )HTML");
  Element* div_element =
      GetDocument().getElementById(AtomicString("first_div"));

  DOMNodeId target_node_id = div_element->GetDomNodeId();
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(target_node_id);
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();

  ASSERT_EQ(range_result, nullptr);
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeNotFound) {
  SetBodyInnerHTML(R"HTML(
    Other text to make sure we are not returning the whole document.
    <div id="first_div">Hello</div>
    Other text to make sure we are not returning the whole document.
  )HTML");
  Element* div_element =
      GetDocument().getElementById(AtomicString("first_div"));

  DOMNodeId target_node_id =
      div_element->GetDomNodeId() + 9999;  // a non-existent node id
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(target_node_id);
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();

  ASSERT_EQ(range_result, nullptr);
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeIsDisconnected) {
  SetBodyInnerHTML(R"HTML(
    Other text to make sure we are not returning the whole document.
    <div id="first_div">
        <p>Hello</p><p>World</p>
    </div>
    Other text to make sure we are not returning the whole document.
  )HTML");
  Element* div_element =
      GetDocument().getElementById(AtomicString("first_div"));

  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(div_element->GetDomNodeId());
  div_element->remove();
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();
  EXPECT_EQ(range_result, nullptr);
}

TEST_F(NodeAnnotationSelectorTest, FindRangeNodeFromDifferentDocument) {
  SetBodyInnerHTML(R"HTML(
    <iframe></iframe>
    <div>some text</div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="first_div">
      <p>Hello</p><p>World</p>
    </div>
  )HTML");
  Element* div_element =
      ChildDocument().getElementById(AtomicString("first_div"));
  ASSERT_NE(div_element->GetDocument(), GetDocument());

  // Note that we use div_element's (which is in the iframe's document) ID, and
  // the main frame's document.
  NodeAnnotationSelector* selector =
      MakeGarbageCollected<NodeAnnotationSelector>(div_element->GetDomNodeId());
  base::test::TestFuture<const RangeInFlatTree*> future;
  selector->FindRange(*DocumentBodyRange(),
                      AnnotationSelector::SearchType::kSynchronous,
                      future.GetCallback());

  const RangeInFlatTree* range_result = future.Take();
  EXPECT_EQ(range_result, nullptr);
}

}  // namespace blink
