// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using testing::Return;
using testing::MatchesRegex;

class LayoutObjectTest : public RenderingTest {
 public:
  LayoutObjectTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()) {}

 protected:
  template <bool should_have_wrapper>
  void ExpectAnonymousInlineWrapperFor(Node*);
};

template <bool should_have_wrapper>
void LayoutObjectTest::ExpectAnonymousInlineWrapperFor(Node* node) {
  ASSERT_TRUE(node);
  EXPECT_TRUE(node->IsTextNode());
  LayoutObject* text_layout = node->GetLayoutObject();
  ASSERT_TRUE(text_layout);
  LayoutObject* text_parent = text_layout->Parent();
  ASSERT_TRUE(text_parent);
  if (should_have_wrapper) {
    EXPECT_TRUE(text_parent->IsAnonymous());
    EXPECT_TRUE(text_parent->IsInline());
  } else {
    EXPECT_FALSE(text_parent->IsAnonymous());
  }
}

TEST_F(LayoutObjectTest, LayoutDecoratedNameCalledWithPositionedObject) {
  SetBodyInnerHTML("<div id='div' style='position: fixed'>test</div>");
  Element* div = GetDocument().getElementById(AtomicString("div"));
  DCHECK(div);
  LayoutObject* obj = div->GetLayoutObject();
  DCHECK(obj);
  EXPECT_THAT(obj->DecoratedName().Ascii(),
              MatchesRegex("LayoutN?G?BlockFlow \\(positioned\\)"));
}

// Some display checks.
TEST_F(LayoutObjectTest, DisplayNoneCreateObject) {
  SetBodyInnerHTML("<div style='display:none'></div>");
  EXPECT_EQ(nullptr, GetDocument().body()->firstChild()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, DisplayBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_FALSE(layout_object->IsInline());
}

TEST_F(LayoutObjectTest, DisplayInlineBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:inline-block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_TRUE(layout_object->IsInline());
}

TEST_F(LayoutObjectTest, BackdropFilterAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="backdrop-filter: blur(2px)"></div>
    <div id=target2 style="will-change: backdrop-filter"></div>
    <div id=target3 style="position: relative"></div>
  )HTML");
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target3")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target3")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, BlendModeAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="mix-blend-mode: multiply"></div>
    <div id=target2 style="position: relative"></div>
  )HTML");
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target2")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, CSSClipAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="clip: rect(1px, 2px, 3px, 4px)"></div>
    <div id=target2 style="position: absolute; clip: rect(1px, 2px, 3px, 4px)">
    </div>
    <div id=target3 style="position: relative"></div>
  )HTML");
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target3")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target3")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, ClipPathAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="clip-path: circle(40%)"></div>
    <div id=target2 style="position: relative"></div>
  )HTML");
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target2")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, IsolationAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="isolation: isolate"></div>
    <div id=target2 style="position: relative"></div>
  )HTML");
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target2")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, MaskAsGroupingProperty) {
  SetBodyInnerHTML(R"HTML(
    <style> div { transform-style: preserve-3d; } </style>
    <div id=target1 style="-webkit-mask:linear-gradient(black,transparent)">
    </div>
    <div id=target2 style="position: relative"></div>
  )HTML");
  EXPECT_TRUE(GetLayoutObjectByElementId("target1")
                  ->StyleRef()
                  .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->StyleRef().Preserves3D());

  EXPECT_FALSE(GetLayoutObjectByElementId("target2")
                   ->StyleRef()
                   .HasGroupingPropertyForUsedTransformStyle3D());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")->StyleRef().Preserves3D());
}

TEST_F(LayoutObjectTest, UseCountContainWithoutContentVisibility) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .cv { content-visibility: auto }
      .strict { contain: strict }
      .all { contain: size paint layout style }
    </style>
    <div id=target class=cv></div>
  )HTML");
  auto* target = GetDocument().getElementById("target");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainAllWithoutContentVisibility));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainStrictWithoutContentVisibility));

  target->classList().Add("all");
  UpdateAllLifecyclePhasesForTest();

  // With content-visibility, we don't count the features.
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainAllWithoutContentVisibility));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainStrictWithoutContentVisibility));

  target->classList().Remove("cv");
  target->classList().Remove("all");
  target->classList().Add("strict");
  UpdateAllLifecyclePhasesForTest();

  // Strict should register, but without style containment the "all" bucket is
  // not counted.
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainAllWithoutContentVisibility));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainStrictWithoutContentVisibility));

  target->classList().Remove("strict");
  target->classList().Add("all");
  UpdateAllLifecyclePhasesForTest();

  // Everything should be counted now.
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainAllWithoutContentVisibility));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSContainStrictWithoutContentVisibility));
}

TEST_F(LayoutObjectTest, UseCountContainingBlockFixedPosUnderFlattened3D) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform-style: preserve-3d; opacity: 0.9'>
      <div id=target style='position:fixed'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(target->View(), target->Container());

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kTransformStyleContainingBlockComputedUsedMismatch));
}

// Containing block test.
TEST_F(LayoutObjectTest, ContainingBlockLayoutViewShouldBeNull) {
  EXPECT_EQ(nullptr, GetLayoutView().ContainingBlock());
}

TEST_F(LayoutObjectTest, ContainingBlockBodyShouldBeDocumentElement) {
  EXPECT_EQ(GetDocument().body()->GetLayoutObject()->ContainingBlock(),
            GetDocument().documentElement()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, ContainingBlockDocumentElementShouldBeLayoutView) {
  EXPECT_EQ(
      GetDocument().documentElement()->GetLayoutObject()->ContainingBlock(),
      GetLayoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockStaticLayoutObjectShouldBeParent) {
  SetBodyInnerHTML("<foo style='position:static'></foo>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* layout_object = body_layout_object->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
}

TEST_F(LayoutObjectTest,
       ContainingBlockAbsoluteLayoutObjectShouldBeLayoutView) {
  SetBodyInnerHTML("<foo style='position:absolute'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldBeNonStaticallyPositionedBlockAncestor) {
  SetBodyInnerHTML(R"HTML(
    <div style='position:relative; left:20px'>
      <bar style='position:absolute; left:2px; top:10px'></bar>
    </div>
  )HTML");
  LayoutObject* containing_blocklayout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  LayoutObject* layout_object = containing_blocklayout_object->SlowFirstChild();
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kAbsolute));
  EXPECT_FALSE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
  auto offset =
      layout_object->OffsetFromContainer(containing_blocklayout_object);
  EXPECT_EQ(PhysicalOffset(2, 10), offset);
}

TEST_F(LayoutObjectTest, ContainingBlockFixedPosUnderFlattened3DWithInterop) {
  ScopedTransformInteropForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <div id=container style='transform-style: preserve-3d; opacity: 0.9'>
      <div id=target style='position:fixed'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_EQ(container, target->Container());
}

TEST_F(LayoutObjectTest, ContainingBlockFixedLayoutObjectInTransformedDiv) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform:translateX(0px)'>
      <bar style='position:fixed'></bar>
    </div>
  )HTML");
  LayoutObject* containing_blocklayout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  LayoutObject* layout_object = containing_blocklayout_object->SlowFirstChild();
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kAbsolute));
  EXPECT_TRUE(
      containing_blocklayout_object->CanContainOutOfFlowPositionedElement(
          EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            containing_blocklayout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(),
            containing_blocklayout_object);
}

TEST_F(LayoutObjectTest, ContainingBlockFixedLayoutObjectInBody) {
  SetBodyInnerHTML("<div style='position:fixed'></div>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_TRUE(layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(
      layout_object->CanContainOutOfFlowPositionedElement(EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockAbsoluteLayoutObjectInBody) {
  SetBodyInnerHTML("<div style='position:absolute'></div>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_TRUE(layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(
      layout_object->CanContainOutOfFlowPositionedElement(EPosition::kFixed));
  EXPECT_EQ(layout_object->Container(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            GetLayoutView());
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldNotBeNonStaticallyPositionedInlineAncestor) {
  // Test note: We can't use a raw string literal here, since extra whitespace
  // causes failures.
  SetBodyInnerHTML(
      "<span style='position:relative; top:1px; left:2px'><bar "
      "style='position:absolute; top:10px; left:20px;'></bar></span>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* span_layout_object = body_layout_object->SlowFirstChild();
  LayoutObject* layout_object = span_layout_object->SlowFirstChild();

  EXPECT_TRUE(span_layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kAbsolute));
  EXPECT_FALSE(span_layout_object->CanContainOutOfFlowPositionedElement(
      EPosition::kFixed));

  auto offset = layout_object->OffsetFromContainer(span_layout_object);
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_EQ(PhysicalOffset(22, 11), offset);
  else
    EXPECT_EQ(PhysicalOffset(20, 10), offset);

  // Sanity check: Make sure we don't generate anonymous objects.
  EXPECT_EQ(nullptr, body_layout_object->SlowFirstChild()->NextSibling());
  EXPECT_EQ(nullptr, layout_object->SlowFirstChild());
  EXPECT_EQ(nullptr, layout_object->NextSibling());

  EXPECT_EQ(layout_object->Container(), span_layout_object);
  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
  EXPECT_EQ(layout_object->ContainingBlockForAbsolutePosition(),
            body_layout_object);
  EXPECT_EQ(layout_object->ContainingBlockForFixedPosition(), GetLayoutView());
}

TEST_F(LayoutObjectTest, PaintingLayerOfOverflowClipLayerUnderColumnSpanAll) {
  SetBodyInnerHTML(R"HTML(
    <div id='columns' style='columns: 3'>
      <div style='column-span: all'>
        <div id='overflow-clip-layer' style='height: 100px; overflow:
    hidden'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* overflow_clip_object =
      GetLayoutObjectByElementId("overflow-clip-layer");
  LayoutBlock* columns = To<LayoutBlock>(GetLayoutObjectByElementId("columns"));
  EXPECT_EQ(columns->Layer(), overflow_clip_object->PaintingLayer());
}

TEST_F(LayoutObjectTest, FloatUnderBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <div id='floating' style='float: left'>FLOAT</div>
      </div>
    </div>
  )HTML");

  auto* layered_div =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("layered-div"));
  auto* container =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("container"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
  EXPECT_EQ(container, floating->Container());
  EXPECT_EQ(container, floating->ContainingBlock());
}

TEST_F(LayoutObjectTest, InlineFloatMismatch) {
  SetBodyInnerHTML(R"HTML(
    <span id=span style='position: relative; left: 40px; width: 100px; height: 100px'>
      <div id=float_obj style='float: left; margin-left: 10px;'>
      </div>
    </span>
  )HTML");

  LayoutObject* float_obj = GetLayoutObjectByElementId("float_obj");
  LayoutObject* span = GetLayoutObjectByElementId("span");
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // 10px for margin + 40px for inset.
    EXPECT_EQ(PhysicalOffset(50, 0), float_obj->OffsetFromAncestor(span));
  } else {
    // 10px for margin, -40px because float is to the left of the span.
    EXPECT_EQ(PhysicalOffset(-30, 0), float_obj->OffsetFromAncestor(span));
  }
}

TEST_F(LayoutObjectTest, FloatUnderInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <span id='layered-span' style='position: relative'>
          <div id='floating' style='float: left'>FLOAT</div>
        </span>
      </div>
    </div>
  )HTML");

  auto* layered_div =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("layered-div"));
  auto* container =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("container"));
  auto* layered_span =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("layered-span"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_span->Layer(), layered_span->PaintingLayer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // LayoutNG inline-level floats are children of their inline-level
    // containers. As such LayoutNG paints these within the correct
    // inline-level layer.
    EXPECT_EQ(layered_span->Layer(), floating->PaintingLayer());
    EXPECT_EQ(layered_span, floating->Container());
  } else {
    EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
    EXPECT_EQ(container, floating->Container());
  }
  EXPECT_EQ(container, floating->ContainingBlock());

  LayoutObject::AncestorSkipInfo skip_info(layered_span);
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_EQ(layered_span, floating->Container(&skip_info));
    EXPECT_FALSE(skip_info.AncestorSkipped());
  } else {
    EXPECT_EQ(container, floating->Container(&skip_info));
    EXPECT_TRUE(skip_info.AncestorSkipped());

    skip_info = LayoutObject::AncestorSkipInfo(container);
    EXPECT_EQ(container, floating->Container(&skip_info));
    EXPECT_FALSE(skip_info.AncestorSkipped());
  }
}

TEST_F(LayoutObjectTest, MutableForPaintingClearPaintFlags) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->ShouldCheckGeometryForPaintInvalidation());
  object->SetShouldCheckForPaintInvalidation();
  EXPECT_TRUE(object->ShouldCheckForPaintInvalidation());
  object->SetSubtreeShouldCheckForPaintInvalidation();
  EXPECT_TRUE(object->SubtreeShouldCheckForPaintInvalidation());
  object->SetMayNeedPaintInvalidationAnimatedBackgroundImage();
  EXPECT_TRUE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  object->SetShouldInvalidateSelection();
  EXPECT_TRUE(object->ShouldInvalidateSelection());
  object->SetBackgroundNeedsFullPaintInvalidation();
  EXPECT_TRUE(object->BackgroundNeedsFullPaintInvalidation());
  object->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());
  object->bitfields_.SetDescendantNeedsPaintPropertyUpdate(true);
  EXPECT_TRUE(object->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->SubtreeShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  EXPECT_FALSE(object->ShouldInvalidateSelection());
  EXPECT_FALSE(object->BackgroundNeedsFullPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(object->DescendantNeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, SubtreePaintPropertyUpdateReasons) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->AddSubtreePaintPropertyUpdateReason(
      SubtreePaintPropertyUpdateReason::kFragmentsChanged);
  EXPECT_TRUE(object->SubtreePaintPropertyUpdateReasons());
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->SubtreePaintPropertyUpdateReasons());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, ShouldCheckGeometryForPaintInvalidation) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  LayoutObject* parent = object->Parent();

  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());

  object->SetShouldCheckForPaintInvalidation();
  EXPECT_TRUE(object->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());

  object->SetShouldDoFullPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->SetShouldCheckForPaintInvalidation();
  EXPECT_TRUE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());

  object->SetShouldCheckForPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->SetShouldCheckForPaintInvalidation();
  EXPECT_TRUE(object->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->ShouldCheckGeometryForPaintInvalidation());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(parent->DescendantShouldCheckGeometryForPaintInvalidation());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterPunctuations) {
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>(a)bc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const auto* layout_object0 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const auto* layout_object1 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_EQ(layout_object0, layout_object1)
      << "A character 'a' should be part of first letter.";

  const auto* layout_object2 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object0, layout_object2)
      << "close parenthesis should be part of first letter.";

  const auto* layout_object3 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 3));
  EXPECT_TRUE(layout_object3->IsRemainingTextLayoutObject());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  To<Text>(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  const auto* layout_object0 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*first_letter, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const auto* layout_object1 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*first_letter, 1));
  EXPECT_EQ(layout_object0, layout_object1);
}

TEST_F(LayoutObjectTest,
       AssociatedLayoutObjectOfFirstLetterWithTrailingWhitespace) {
  const char* body_content = R"HTML(
    <style>
      div:first-letter {
        color:red;
      }
    </style>
    <div id=sample>a
      <div></div>
    </div>
  )HTML";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const auto* layout_object0 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const auto* layout_object1 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_TRUE(layout_object1->IsRemainingTextLayoutObject());

  const auto* layout_object2 =
      To<LayoutTextFragment>(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object1, layout_object2);
}

TEST_F(LayoutObjectTest, VisualRect) {
  class MockLayoutObject : public LayoutObject {
   public:
    MockLayoutObject() : LayoutObject(nullptr) {}
    ~MockLayoutObject() override { SetBeingDestroyedForTesting(); }
    MOCK_CONST_METHOD0(VisualRectRespectsVisibility, bool());

   private:
    PhysicalRect LocalVisualRectIgnoringVisibility() const override {
      return PhysicalRect(10, 10, 20, 20);
    }
    const char* GetName() const final { return "MockLayoutObject"; }
    void UpdateLayout() final {}
    FloatRect LocalBoundingBoxRectForAccessibility() const final {
      return FloatRect();
    }
  };

  MockLayoutObject mock_object;
  auto style = ComputedStyle::Create();
  mock_object.SetStyle(style.get());
  EXPECT_EQ(PhysicalRect(10, 10, 20, 20), mock_object.LocalVisualRect());
  EXPECT_EQ(PhysicalRect(10, 10, 20, 20), mock_object.LocalVisualRect());

  style->SetVisibility(EVisibility::kHidden);
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(true));
  EXPECT_TRUE(mock_object.LocalVisualRect().IsEmpty());
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(false));
  EXPECT_EQ(PhysicalRect(10, 10, 20, 20), mock_object.LocalVisualRect());
}

TEST_F(LayoutObjectTest, DisplayContentsInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsNoInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsAddInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);

  div->SetInlineStyleProperty(CSSPropertyID::kColor, "pink");
  UpdateAllLifecyclePhasesForTest();
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsRemoveInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);

  div->RemoveInlineStyleProperty(CSSPropertyID::kColor);
  UpdateAllLifecyclePhasesForTest();
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperPerTextNode) {
  // This test checks the current implementation; that text node siblings do not
  // share inline wrappers. Doing so requires code to handle all situations
  // where text nodes are no longer layout tree siblings by splitting wrappers,
  // and merge wrappers when text nodes become layout tree siblings.
  SetBodyInnerHTML(
      "<div id='div' style='display:contents;color:pink'>A<!-- -->B</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text1 = div->firstChild();
  ASSERT_TRUE(text1);
  Node* text2 = div->lastChild();
  ASSERT_TRUE(text2);
  EXPECT_NE(text1, text2);

  ExpectAnonymousInlineWrapperFor<true>(text1);
  ExpectAnonymousInlineWrapperFor<true>(text2);

  EXPECT_NE(text1->GetLayoutObject()->Parent(),
            text2->GetLayoutObject()->Parent());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTable) {
  SetBodyInnerHTML(R"HTML(
    <div id='table' style='display:table'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableSection) {
  SetBodyInnerHTML(R"HTML(
    <div id='section' style='display:table-row-group'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableRow) {
  SetBodyInnerHTML(R"HTML(
    <div id='row' style='display:table-row'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableCell) {
  SetBodyInnerHTML(R"HTML(
    <div id='cell' style='display:table-cell'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* cell = GetDocument().getElementById("cell");
  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(none->GetLayoutObject());
  EXPECT_EQ(cell->GetLayoutObject(), none->GetLayoutObject()->Parent());
}

#if DCHECK_IS_ON()
TEST_F(LayoutObjectTest, DumpLayoutObject) {
  // Test dumping for debugging, in particular that newlines and non-ASCII
  // characters are escaped as expected.
  SetBodyInnerHTML(String::FromUTF8(R"HTML(
    <div id='block' style='background:
lime'>
      testing Среќен роденден
</div>
  )HTML"));

  LayoutObject* block = GetLayoutObjectByElementId("block");
  ASSERT_TRUE(block);
  LayoutObject* text = block->SlowFirstChild();
  ASSERT_TRUE(text);

  StringBuilder result;
  block->DumpLayoutObject(result, false, 0);
  EXPECT_THAT(result.ToString().Utf8(),
              MatchesRegex("LayoutN?G?BlockFlow\tDIV id=\"block\" "
                           "style=\"background:\\\\nlime\""));

  result.Clear();
  text->DumpLayoutObject(result, false, 0);
  EXPECT_EQ(
      result.ToString(),
      String("LayoutText\t#text \"\\n      testing "
             "\\u0421\\u0440\\u0435\\u045C\\u0435\\u043D "
             "\\u0440\\u043E\\u0434\\u0435\\u043D\\u0434\\u0435\\u043D\\n\""));
}
#endif  // DCHECK_IS_ON()

TEST_F(LayoutObjectTest, DisplayContentsSVGGElementInHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>*|g { display:contents}</style>
    <span id=span></span>
  )HTML");

  Element* span = GetDocument().getElementById("span");
  auto* svg_element = MakeGarbageCollected<SVGGElement>(GetDocument());
  Text* text = Text::Create(GetDocument(), "text");
  svg_element->appendChild(text);
  span->appendChild(svg_element);

  UpdateAllLifecyclePhasesForTest();

  ASSERT_FALSE(svg_element->GetLayoutObject());
  ASSERT_FALSE(text->GetLayoutObject());
}

TEST_F(LayoutObjectTest, HasDistortingVisualEffects) {
  SetBodyInnerHTML(R"HTML(
    <div id=opaque style='opacity:1'><div class=inner></div></div>
    <div id=transparent style='opacity:0.99'><div class=inner></div></div>
    <div id=blurred style='filter:blur(5px)'><div class=inner></div></div>
    <div id=blended style='mix-blend-mode:hue'><div class=inner></div></div>
    <div id=good-transform style='transform:translateX(10px) scale(1.6)'>
      <div class=inner></div>
    </div>
    <div id=bad-transform style='transform:rotate(45deg)'>
      <div class=inner></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* outer = GetDocument().getElementById("opaque");
  Element* inner = outer->QuerySelector(".inner");
  ASSERT_FALSE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("transparent");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("blurred");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("blended");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("good-transform");
  inner = outer->QuerySelector(".inner");
  ASSERT_FALSE(inner->GetLayoutObject()->HasDistortingVisualEffects());

  outer = GetDocument().getElementById("bad-transform");
  inner = outer->QuerySelector(".inner");
  ASSERT_TRUE(inner->GetLayoutObject()->HasDistortingVisualEffects());
}

TEST_F(LayoutObjectTest, DistortingVisualEffectsUnaliases) {
  SetBodyInnerHTML(R"HTML(
    <div style="opacity: 0.2;">
      <div style="width: 100px height:100px; contain: paint">
        <div id="child"
             style="position: relative; width: 100px; height:100px;"></div>
      </div>
    </div>
  )HTML");

  const auto* child = GetDocument().getElementById("child");
  const auto* object = child->GetLayoutObject();
  // This should pass and not DCHECK if the nodes are unaliased correctly.
  EXPECT_TRUE(object->HasDistortingVisualEffects());
  EXPECT_TRUE(object->HasNonZeroEffectiveOpacity());
}

TEST_F(LayoutObjectTest, UpdateVisualRectAfterAncestorLayout) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 50px;
        height: 0;
        position: relative;
      }
    </style>
    <div id=ancestor style="width: 100px; height: 100px; position: relative">
      <div>
        <div id=target></div>
      </div>
    </div>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr, "height: 300px");
  UpdateAllLifecyclePhasesForTest();
  const auto* container = GetLayoutBoxByElementId("ancestor");
  EXPECT_EQ(LayoutRect(0, 0, 100, 300), container->VisualOverflowRect());
}

class LayoutObjectSimTest : public SimTest {
 public:
  bool DocumentHasTouchActionRegion(const EventHandlerRegistry& registry) {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return registry.HasEventHandlers(
        EventHandlerRegistry::EventHandlerClass::kTouchAction);
  }
};

TEST_F(LayoutObjectSimTest, TouchActionUpdatesSubframeEventHandler) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<div id='container'>"
      "<iframe src=frame.html></iframe>"
      "</div>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>"
      "<div id='inner'></div>"
      "</body></html>");

  Element* iframe_element = GetDocument().QuerySelector("iframe");
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(iframe_element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  Element* inner = iframe_doc->getElementById("inner");
  Element* iframe_doc_element = iframe_doc->documentElement();
  Element* container = GetDocument().getElementById("container");

  EventHandlerRegistry& registry =
      iframe_doc->GetFrame()->GetEventHandlerRegistry();

  // We should add event handler if touch action is set on subframe.
  inner->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should remove event handler if touch action is removed on subframe.
  inner->setAttribute("style", "touch-action: auto");
  EXPECT_FALSE(DocumentHasTouchActionRegion(registry));

  // We should add event handler if touch action is set on main frame.
  container->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should keep event handler if touch action is set on subframe document
  // element.
  iframe_doc_element->setAttribute("style", "touch-action: none");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should keep the event handler if touch action is removed on subframe
  // document element.
  iframe_doc_element->setAttribute("style", "touch-action: auto");
  EXPECT_TRUE(DocumentHasTouchActionRegion(registry));

  // We should remove the handler if touch action is removed on main frame.
  container->setAttribute("style", "touch-action: auto");
  EXPECT_FALSE(DocumentHasTouchActionRegion(registry));
}

TEST_F(LayoutObjectSimTest, HitTestForOcclusionInIframe) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <iframe style='width:300px;height:150px;' src=frame.html></iframe>
    <div id='occluder' style='will-change:transform;width:100px;height:100px;'>
    </div>
  )HTML");
  frame_resource.Complete(R"HTML(
    <div id='target'>target</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* iframe_element = GetDocument().QuerySelector("iframe");
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(iframe_element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  Element* target = iframe_doc->getElementById("target");
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  Element* occluder = GetDocument().getElementById("occluder");
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-150px");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

TEST_F(LayoutObjectSimTest, FirstLineBackgroundImage) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <style>
      div::first-line {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
      span { background: rgba(0, 255, 0, 0.3); }
    </style>
    <div id="target">
      <span id="first-line1">Text</span><span id="first-line2">Text</span><br>
      <span id="second-line">Text</span>
    </div>
    <div>To keep the image alive when target is set display: none</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* target = GetDocument().getElementById("target");
  auto* target_object = target->GetLayoutObject();
  auto* image_resource_content = target_object->FirstLineStyleRef()
                                     .BackgroundLayers()
                                     .GetImage()
                                     ->CachedImage();

  // Simulate an image change notification, and we should invalidate the objects
  // in the first line.
  static_cast<ImageObserver*>(image_resource_content)
      ->Changed(image_resource_content->GetImage());

  // The block is the layout object of the first line's root line box, so we
  // invalidate it.
  EXPECT_TRUE(target_object->ShouldDoFullPaintInvalidation());

  auto* first_line1 =
      GetDocument().getElementById("first-line1")->GetLayoutObject();
  EXPECT_TRUE(first_line1->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(first_line1->SlowFirstChild()->ShouldDoFullPaintInvalidation());
  auto* first_line2 =
      GetDocument().getElementById("first-line2")->GetLayoutObject();
  EXPECT_TRUE(first_line2->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(first_line2->SlowFirstChild()->ShouldDoFullPaintInvalidation());
  auto* second_line =
      GetDocument().getElementById("second-line")->GetLayoutObject();
  EXPECT_FALSE(second_line->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(second_line->SlowFirstChild()->ShouldDoFullPaintInvalidation());

  target->setAttribute(html_names::kStyleAttr, "display: none");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  target_object = target->GetLayoutObject();
  EXPECT_EQ(nullptr, target_object);
  // The image is still alive because the other div's first line style still
  // reference it. The following statement should not crash.
  static_cast<ImageObserver*>(image_resource_content)
      ->Changed(image_resource_content->GetImage());
}

TEST_F(LayoutObjectTest, FirstLineBackgroundImageNestedCrash) {
  SetBodyInnerHTML(R"HTML(
    <style>
      *::first-line { background-image: linear-gradient(red, blue); }
    </style>
    <div><span><div>ABCDE</div></span></div>
  )HTML");

  // The following code should not crash due to incorrectly paired
  // StyleImage::AddClient() and RemoveClient().
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "display: none");
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(LayoutObjectTest, FirstLineBackgroundImageAddBlockBackgroundImageCrash) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target::first-line { background-image: linear-gradient(red, blue); }
    </style>
    <div id="target"></div>
  )HTML");

  // The following code should not crash due to incorrectly paired
  // StyleImage::AddClient() and RemoveClient().
  GetDocument().getElementById("target")->setAttribute(
      html_names::kStyleAttr,
      "background-image: url(data:image/gif;base64,"
      "R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==)");
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(LayoutObjectTest, FirstLineBackgroundImageChangeStyleCrash) {
  SetBodyInnerHTML(R"HTML(
    <style id="style">
      #target::first-line {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div id="target">Target</div>
  )HTML");

  // These should not crash.
  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "color: blue");
  UpdateAllLifecyclePhasesForTest();

  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "display: none");
  UpdateAllLifecyclePhasesForTest();

  auto* style_element = GetDocument().getElementById("style");
  style_element->setTextContent(style_element->textContent() + "dummy");
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(LayoutObjectSimTest, FirstLineBackgroundImageDirtyStyleCrash) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <style id="style">
      #target { display: list-item; }
      div::first-line {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div id="target">Text</div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  CSSStyleSheet* sheet =
      To<HTMLStyleElement>(GetDocument().getElementById("style"))->sheet();
  {
    // "Mutate" the rules to clear the StyleSheetContents RuleSet member.
    CSSStyleSheet::RuleMutationScope scope(sheet);
  }
  EXPECT_FALSE(sheet->Contents()->HasRuleSet());

  auto* target = GetDocument().getElementById("target");
  auto* target_object = target->GetLayoutObject();
  auto* image_resource_content = target_object->FirstLineStyleRef()
                                     .BackgroundLayers()
                                     .GetImage()
                                     ->CachedImage();
  auto* image = image_resource_content->GetImage();
  auto* image_observer = static_cast<ImageObserver*>(image_resource_content);

  // LayoutBlock::ImageChanged() will be triggered which makes us look up the
  // ::first-line style before marking for paint invalidation. We should not try
  // to compute style if it doesn't exist. The first invocation will mark for
  // paint invalidation which will clear the cached ::first-line styles.
  image_observer->Changed(image);
  EXPECT_TRUE(target_object->ShouldDoFullPaintInvalidation());

  // For the second invocation, the ::first-line styles is null. If we try to
  // compute the styles here, we will crash since the RuleSet is null and we
  // need an active style update.
  image_observer->Changed(image);
  EXPECT_TRUE(target_object->ShouldDoFullPaintInvalidation());
}

TEST_F(LayoutObjectTest, NeedsLayoutOverflowRecalc) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='wrapper'>
      <div id='target'>foo</div>
      <div id='other'>bar</div>
    </div>
  )HTML");

  auto* wrapper = GetLayoutObjectByElementId("wrapper");
  auto* target = GetLayoutObjectByElementId("target");
  auto* other = GetLayoutObjectByElementId("other");

  DCHECK(wrapper);
  DCHECK(target);
  DCHECK(other);

  EXPECT_FALSE(wrapper->NeedsLayoutOverflowRecalc());
  EXPECT_FALSE(target->NeedsLayoutOverflowRecalc());
  EXPECT_FALSE(other->NeedsLayoutOverflowRecalc());

  auto* target_element = GetDocument().getElementById("target");
  target_element->setInnerHTML("baz");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(wrapper->NeedsLayoutOverflowRecalc());
  EXPECT_FALSE(target->NeedsLayoutOverflowRecalc());
  EXPECT_FALSE(other->NeedsLayoutOverflowRecalc());
}

TEST_F(LayoutObjectTest, ContainValueIsRelayoutBoundary) {
  SetBodyInnerHTML(R"HTML(
    <div id='target1' style='contain:layout'></div>
    <div id='target2' style='contain:layout size'></div>
    <div id='target3' style='contain:paint'></div>
    <div id='target4' style='contain:size'></div>
    <div id='target5' style='contain:content'></div>
    <div id='target6' style='contain:strict'></div>
  )HTML");
  EXPECT_FALSE(GetLayoutObjectByElementId("target1")->IsRelayoutBoundary());
  EXPECT_TRUE(GetLayoutObjectByElementId("target2")->IsRelayoutBoundary());
  EXPECT_FALSE(GetLayoutObjectByElementId("target3")->IsRelayoutBoundary());
  EXPECT_FALSE(GetLayoutObjectByElementId("target4")->IsRelayoutBoundary());
  EXPECT_FALSE(GetLayoutObjectByElementId("target5")->IsRelayoutBoundary());
  EXPECT_TRUE(GetLayoutObjectByElementId("target6")->IsRelayoutBoundary());
}

TEST_F(LayoutObjectTest, PerspectiveIsNotParent) {
  ScopedTransformInteropForTest enabled(true);

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin:0; }</style>
    <div id='ancestor' style='perspective: 100px'>
      <div>
        <div id='child' style='width: 10px; height: 10px; transform: rotateY(45deg);
        position: absolute'></div>
      </div>
    </div>
  )HTML");

  auto* ancestor = GetLayoutBoxByElementId("ancestor");
  auto* child = GetLayoutBoxByElementId("child");

  TransformationMatrix transform;
  child->GetTransformFromContainer(ancestor, PhysicalOffset(), transform);
  TransformationMatrix::DecomposedType decomposed;
  EXPECT_TRUE(transform.Decompose(decomposed));
  EXPECT_EQ(0, decomposed.perspective_z);
}

TEST_F(LayoutObjectTest, PerspectiveWithAnonymousTable) {
  ScopedTransformInteropForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>body { margin:0; }</style>
    <div id='ancestor' style='display: table; perspective: 100px; width: 100px; height: 100px;'>
      <div id='child' style='display: table-cell; width: 100px; height: 100px; transform: rotateY(45deg);
        position: absolute'></div>
    </table>
  )HTML");

  LayoutObject* child = GetLayoutObjectByElementId("child");
  auto* ancestor =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("ancestor"));

  TransformationMatrix transform;
  child->GetTransformFromContainer(ancestor, PhysicalOffset(), transform);
  TransformationMatrix::DecomposedType decomposed;
  EXPECT_TRUE(transform.Decompose(decomposed));
  EXPECT_EQ(-0.01, decomposed.perspective_z);
}

TEST_F(LayoutObjectTest, LocalToAncestorRectFastPath) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin:0; }</style>
    <div id=target
         style="transform: translate(50px, 25px); width: 10px; height: 10px">
    </div>
    <div id=ancestor2 style="position: relative">
      <div id=target2
         style="transform: translate(75px, 15px); width: 10px; height: 10px">
      </div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  PhysicalRect rect(0, 0, 10, 10);
  PhysicalRect result;

  EXPECT_TRUE(target->LocalToAncestorRectFastPath(
      rect, nullptr, kUseGeometryMapperMode, result));
  EXPECT_EQ(PhysicalRect(50, 25, 10, 10), result);
  // Compare with non-fast path.
  EXPECT_EQ(PhysicalRect(50, 25, 10, 10),
            target->LocalToAncestorRect(rect, nullptr));

  // No other modes are supported.
  EXPECT_FALSE(target->LocalToAncestorRectFastPath(rect, nullptr, 0, result));
  EXPECT_FALSE(
      target->LocalToAncestorRectFastPath(rect, nullptr, kIsFixed, result));
  EXPECT_FALSE(target->LocalToAncestorRectFastPath(rect, nullptr,
                                                   kIgnoreTransforms, result));
  EXPECT_FALSE(target->LocalToAncestorRectFastPath(
      rect, nullptr, kIgnoreStickyOffset, result));
  EXPECT_FALSE(target->LocalToAncestorRectFastPath(
      rect, nullptr, kIgnoreScrollOffset, result));
  EXPECT_FALSE(target->LocalToAncestorRectFastPath(
      rect, nullptr, kApplyRemoteMainFrameTransform, result));

  EXPECT_EQ(PhysicalRect(50, 25, 10, 10),
            target->LocalToAncestorRect(rect, nullptr, kUseGeometryMapperMode));

  LayoutObject* target2 = GetLayoutObjectByElementId("target2");
  LayoutObject* ancestor2 = GetLayoutObjectByElementId("ancestor2");
  PhysicalRect result2;

  EXPECT_TRUE(target2->LocalToAncestorRectFastPath(
      rect, To<LayoutBoxModelObject>(ancestor2), kUseGeometryMapperMode,
      result2));
  EXPECT_EQ(PhysicalRect(75, 15, 10, 10), result2);

  EXPECT_EQ(
      PhysicalRect(75, 15, 10, 10),
      target2->LocalToAncestorRect(rect, To<LayoutBoxModelObject>(ancestor2)));
  // Compare with non-fast path.
  EXPECT_TRUE(target2->LocalToAncestorRectFastPath(
      rect, nullptr, kUseGeometryMapperMode, result2));
  // 25 instead of 15, because #target is 10px high.
  EXPECT_EQ(PhysicalRect(75, 25, 10, 10), result2);

  EXPECT_EQ(PhysicalRect(75, 25, 10, 10),
            target2->LocalToAncestorRect(rect, nullptr));
}

}  // namespace blink
