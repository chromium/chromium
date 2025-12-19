// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_container.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

class ElementTest : public EditingTestBase {
 private:
  ScopedFocusgroupForTest focusgroup_enabled{true};
};

TEST_F(ElementTest, FocusableDesignMode) {
  Document& document = GetDocument();
  DCHECK(IsA<HTMLHtmlElement>(document.documentElement()));
  document.setDesignMode("on");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(document.documentElement()->IsFocusable())
      << "<html> with designMode=on should be focusable.";
}

TEST_F(ElementTest, FocusgroupLastFocusedStorageBasic) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id='container' focusgroup="toolbar">
      <button id='a'>a</button>
      <button id='b'>b</button>
    </div>
  )HTML");

  Element* container = document.getElementById(AtomicString("container"));
  Element* a = document.getElementById(AtomicString("a"));
  Element* b = document.getElementById(AtomicString("b"));

  ASSERT_TRUE(container);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_EQ(nullptr, container->GetFocusgroupLastFocused());

  container->SetFocusgroupLastFocused(*a);
  EXPECT_EQ(a, container->GetFocusgroupLastFocused());

  container->SetFocusgroupLastFocused(*b);
  EXPECT_EQ(b, container->GetFocusgroupLastFocused());
}

TEST_F(ElementTest, FocusgroupLastFocusedUpdatedOnFocus) {
  Document& document = GetDocument();

  SetBodyContent(R"HTML(
    <div id='container' focusgroup="toolbar">
      <button id='a'>a</button>
      <button id='b'>b</button>
    </div>
  )HTML");

  Element* container = document.getElementById(AtomicString("container"));
  Element* a = document.getElementById(AtomicString("a"));
  Element* b = document.getElementById(AtomicString("b"));

  ASSERT_TRUE(container);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  // Initially, no last focused element should be stored.
  EXPECT_EQ(nullptr, container->GetFocusgroupLastFocused());

  // Focus on button 'a' - should update last focused.
  a->Focus();
  EXPECT_EQ(a, container->GetFocusgroupLastFocused());

  // Focus on button 'b' - should update last focused.
  b->Focus();
  EXPECT_EQ(b, container->GetFocusgroupLastFocused());

  // Focus back to button 'a' - should update last focused.
  a->Focus();
  EXPECT_EQ(a, container->GetFocusgroupLastFocused());
}

TEST_F(ElementTest, FocusgroupLastFocusedWeakReference) {
  Document& document = GetDocument();

  SetBodyContent(R"HTML(
    <div id='container' focusgroup="toolbar">
      <button>a</button>
    </div>
  )HTML");

  Element* container = document.getElementById(AtomicString("container"));
  ASSERT_TRUE(container);

  Element* button = document.CreateElementForBinding(AtomicString("button"));
  container->appendChild(button);

  // Set the last focused element.
  container->SetFocusgroupLastFocused(*button);
  EXPECT_EQ(button, container->GetFocusgroupLastFocused());

  // Remove the button from the tree.
  button->remove();
  button = nullptr;

  // Force garbage collection - the weak reference should allow collection.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  // The weak reference should now return nullptr since the element was garbage
  // collected.
  EXPECT_EQ(nullptr, container->GetFocusgroupLastFocused())
      << "WeakMember should not prevent garbage collection of removed elements";
}

TEST_F(ElementTest,
       GetBoundingClientRectCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <style>body { margin: 0 }
    #scroller { overflow: scroll; height: 100px; width: 100px; }
    #sticky { height: 25px; position: sticky; top: 0; left: 25px; }
    #padding { height: 500px; width: 300px; }</style>
    <div id='scroller'><div id='writer'></div><div id='sticky'></div>
    <div id='padding'></div></div>
  )HTML");

  Element* scroller = document.getElementById(AtomicString("scroller"));
  Element* writer = document.getElementById(AtomicString("writer"));
  Element* sticky = document.getElementById(AtomicString("sticky"));

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollToForTesting(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  DOMRect* bounding_client_rect = sticky->GetBoundingClientRect();
  EXPECT_EQ(0, bounding_client_rect->top());
  EXPECT_EQ(25, bounding_client_rect->left());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting the bounding client rect should cause both layout and
  // compositing inputs clean to be run, and the sticky result shouldn't change.
  bounding_client_rect = sticky->GetBoundingClientRect();
  EXPECT_EQ(DocumentLifecycle::kLayoutClean, document.Lifecycle().GetState());
  EXPECT_EQ(0, bounding_client_rect->top());
  EXPECT_EQ(25, bounding_client_rect->left());
}

TEST_F(ElementTest, OffsetTopAndLeftCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <style>body { margin: 0 }
    #scroller { overflow: scroll; height: 100px; width: 100px; }
    #sticky { height: 25px; position: sticky; top: 0; left: 25px; }
    #padding { height: 500px; width: 300px; }</style>
    <div id='scroller'><div id='writer'></div><div id='sticky'></div>
    <div id='padding'></div></div>
  )HTML");

  Element* scroller = document.getElementById(AtomicString("scroller"));
  Element* writer = document.getElementById(AtomicString("writer"));
  Element* sticky = document.getElementById(AtomicString("sticky"));

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollToForTesting(50.0, 200.0);

  // The sticky element should be offset to stay at (0, 25) relative to the
  // viewport due to the constraints.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->OffsetLeft());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting either offset should cause both layout and compositing inputs
  // clean to be run, and the sticky result shouldn't change.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(DocumentLifecycle::kLayoutClean, document.Lifecycle().GetState());

  // Dirty layout again, since |OffsetTop| will have cleaned it.
  writer->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Again requesting an offset should cause layout and compositing to be clean.
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->OffsetLeft());
  EXPECT_EQ(DocumentLifecycle::kLayoutClean, document.Lifecycle().GetState());
}

TEST_F(ElementTest, BoundsInWidgetCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <style>body { margin: 0 }
    #scroller { overflow: scroll; height: 100px; width: 100px; }
    #sticky { height: 25px; position: sticky; top: 0; left: 25px; }
    #padding { height: 500px; width: 300px; }</style>
    <div id='scroller'><div id='writer'></div><div id='sticky'></div>
    <div id='padding'></div></div>
  )HTML");

  Element* scroller = document.getElementById(AtomicString("scroller"));
  Element* writer = document.getElementById(AtomicString("writer"));
  Element* sticky = document.getElementById(AtomicString("sticky"));

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollToForTesting(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  gfx::Rect bounds_in_viewport = sticky->BoundsInWidget();
  EXPECT_EQ(0, bounds_in_viewport.y());
  EXPECT_EQ(25, bounds_in_viewport.x());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting the bounds in viewport should cause both layout and compositing
  // inputs clean to be run, and the sticky result shouldn't change.
  bounds_in_viewport = sticky->BoundsInWidget();
  EXPECT_EQ(DocumentLifecycle::kLayoutClean, document.Lifecycle().GetState());
  EXPECT_EQ(0, bounds_in_viewport.y());
  EXPECT_EQ(25, bounds_in_viewport.x());
}

TEST_F(ElementTest, OutlineRectsIncludesImgChildren) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <a id='link' href=''><img id='image' width='220' height='147'></a>
  )HTML");

  Element* a = document.getElementById(AtomicString("link"));
  Element* img = document.getElementById(AtomicString("image"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(img);

  // The a element should include the image in computing its bounds.
  gfx::Rect img_bounds_in_viewport = img->BoundsInWidget();
  EXPECT_EQ(220, img_bounds_in_viewport.width());
  EXPECT_EQ(147, img_bounds_in_viewport.height());

  Vector<gfx::Rect> a_outline_rects = a->OutlineRectsInWidget();
  EXPECT_EQ(2u, a_outline_rects.size());

  gfx::Rect a_outline_rect;
  for (auto& r : a_outline_rects)
    a_outline_rect.Union(r);

  EXPECT_EQ(img_bounds_in_viewport.width(), a_outline_rect.width());
  EXPECT_EQ(img_bounds_in_viewport.height(), a_outline_rect.height());
}

TEST_F(ElementTest, StickySubtreesAreTrackedCorrectly) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id='ancestor'>
      <div id='outerSticky' style='position:sticky;'>
        <div id='child'>
          <div id='grandchild'></div>
          <div id='innerSticky' style='position:sticky;'>
            <div id='greatGrandchild'></div>
          </div>
        </div
      </div>
    </div>
  )HTML");

  LayoutObject* ancestor =
      document.getElementById(AtomicString("ancestor"))->GetLayoutObject();
  LayoutObject* outer_sticky =
      document.getElementById(AtomicString("outerSticky"))->GetLayoutObject();
  LayoutObject* child =
      document.getElementById(AtomicString("child"))->GetLayoutObject();
  LayoutObject* grandchild =
      document.getElementById(AtomicString("grandchild"))->GetLayoutObject();
  LayoutObject* inner_sticky =
      document.getElementById(AtomicString("innerSticky"))->GetLayoutObject();
  LayoutObject* great_grandchild =
      document.getElementById(AtomicString("greatGrandchild"))
          ->GetLayoutObject();

  EXPECT_FALSE(ancestor->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(child->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());

  // This forces 'child' to fork it's StyleRareInheritedData, so that we can
  // ensure that the sticky subtree update behavior survives forking.
  document.getElementById(AtomicString("child"))
      ->SetInlineStyleProperty(CSSPropertyID::kWebkitRubyPosition,
                               CSSValueID::kAfter);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(DocumentLifecycle::kPaintClean, document.Lifecycle().GetState());

  EXPECT_EQ(RubyPosition::kOver, outer_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kUnder, child->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kUnder, grandchild->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kUnder, inner_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kUnder,
            great_grandchild->StyleRef().GetRubyPosition());

  // Setting -webkit-ruby value shouldn't have affected the sticky subtree bit.
  EXPECT_TRUE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(child->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());

  // Now switch 'outerSticky' back to being non-sticky - all descendents between
  // it and the 'innerSticky' should be updated, and the 'innerSticky' should
  // fork it's StyleRareInheritedData to maintain the sticky subtree bit.
  document.getElementById(AtomicString("outerSticky"))
      ->SetInlineStyleProperty(CSSPropertyID::kPosition, CSSValueID::kStatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(DocumentLifecycle::kPaintClean, document.Lifecycle().GetState());

  EXPECT_FALSE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_FALSE(child->StyleRef().SubtreeIsSticky());
  EXPECT_FALSE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());
}

TEST_F(ElementTest, GetElementsByClassNameCrash) {
  // Test for a crash in NodeListsNodeData::AddCache().
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  ASSERT_TRUE(GetDocument().InQuirksMode());
  GetDocument().body()->getElementsByClassName(AtomicString("ABC DEF"));
  GetDocument().body()->getElementsByClassName(AtomicString("ABC DEF"));
  // The test passes if no crash happens.
}

TEST_F(ElementTest, GetBoundingClientRectForSVG) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <style>body { margin: 0 }</style>
    <svg width='500' height='500'>
      <rect id='rect' x='10' y='100' width='100' height='71'/>
      <rect id='stroke' x='10' y='100' width='100' height='71'
          stroke-width='7'/>
      <rect id='stroke_transformed' x='10' y='100' width='100' height='71'
          stroke-width='7' transform='translate(3, 5)'/>
      <foreignObject id='foreign' x='10' y='100' width='100' height='71'/>
      <foreignObject id='foreign_transformed' transform='translate(3, 5)'
          x='10' y='100' width='100' height='71'/>
      <svg id='svg' x='10' y='100'>
        <rect width='100' height='71'/>
      </svg>
      <svg id='svg_stroke' x='10' y='100'>
        <rect width='100' height='71' stroke-width='7'/>
      </svg>
    </svg>
  )HTML");

  Element* rect = document.getElementById(AtomicString("rect"));
  DOMRect* rect_bounding_client_rect = rect->GetBoundingClientRect();
  EXPECT_EQ(10, rect_bounding_client_rect->left());
  EXPECT_EQ(100, rect_bounding_client_rect->top());
  EXPECT_EQ(100, rect_bounding_client_rect->width());
  EXPECT_EQ(71, rect_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), rect->BoundsInWidget());

  // TODO(pdr): Should we should be excluding the stroke (here, and below)?
  // See: https://github.com/w3c/svgwg/issues/339 and Element::ClientQuads.
  Element* stroke = document.getElementById(AtomicString("stroke"));
  DOMRect* stroke_bounding_client_rect = stroke->GetBoundingClientRect();
  EXPECT_EQ(10, stroke_bounding_client_rect->left());
  EXPECT_EQ(100, stroke_bounding_client_rect->top());
  EXPECT_EQ(100, stroke_bounding_client_rect->width());
  EXPECT_EQ(71, stroke_bounding_client_rect->height());
  // TODO(pdr): BoundsInWidget is not web exposed and should include
  // stroke.
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), stroke->BoundsInWidget());

  Element* stroke_transformed =
      document.getElementById(AtomicString("stroke_transformed"));
  DOMRect* stroke_transformedbounding_client_rect =
      stroke_transformed->GetBoundingClientRect();
  EXPECT_EQ(13, stroke_transformedbounding_client_rect->left());
  EXPECT_EQ(105, stroke_transformedbounding_client_rect->top());
  EXPECT_EQ(100, stroke_transformedbounding_client_rect->width());
  EXPECT_EQ(71, stroke_transformedbounding_client_rect->height());
  // TODO(pdr): BoundsInWidget is not web exposed and should include
  // stroke.
  EXPECT_EQ(gfx::Rect(13, 105, 100, 71), stroke_transformed->BoundsInWidget());

  Element* foreign = document.getElementById(AtomicString("foreign"));
  DOMRect* foreign_bounding_client_rect = foreign->GetBoundingClientRect();
  EXPECT_EQ(10, foreign_bounding_client_rect->left());
  EXPECT_EQ(100, foreign_bounding_client_rect->top());
  EXPECT_EQ(100, foreign_bounding_client_rect->width());
  EXPECT_EQ(71, foreign_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), foreign->BoundsInWidget());

  Element* foreign_transformed =
      document.getElementById(AtomicString("foreign_transformed"));
  DOMRect* foreign_transformed_bounding_client_rect =
      foreign_transformed->GetBoundingClientRect();
  EXPECT_EQ(13, foreign_transformed_bounding_client_rect->left());
  EXPECT_EQ(105, foreign_transformed_bounding_client_rect->top());
  EXPECT_EQ(100, foreign_transformed_bounding_client_rect->width());
  EXPECT_EQ(71, foreign_transformed_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(13, 105, 100, 71), foreign_transformed->BoundsInWidget());

  Element* svg = document.getElementById(AtomicString("svg"));
  DOMRect* svg_bounding_client_rect = svg->GetBoundingClientRect();
  EXPECT_EQ(10, svg_bounding_client_rect->left());
  EXPECT_EQ(100, svg_bounding_client_rect->top());
  EXPECT_EQ(100, svg_bounding_client_rect->width());
  EXPECT_EQ(71, svg_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), svg->BoundsInWidget());

  Element* svg_stroke = document.getElementById(AtomicString("svg_stroke"));
  DOMRect* svg_stroke_bounding_client_rect =
      svg_stroke->GetBoundingClientRect();
  EXPECT_EQ(10, svg_stroke_bounding_client_rect->left());
  EXPECT_EQ(100, svg_stroke_bounding_client_rect->top());
  EXPECT_EQ(100, svg_stroke_bounding_client_rect->width());
  EXPECT_EQ(71, svg_stroke_bounding_client_rect->height());
  // TODO(pdr): BoundsInWidget is not web exposed and should include
  // stroke.
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), svg_stroke->BoundsInWidget());
}

TEST_F(ElementTest, PartAttribute) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <span id='has_one_part' part='partname'></span>
    <span id='has_two_parts' part='partname1 partname2'></span>
    <span id='has_no_part'></span>
  )HTML");

  Element* has_one_part = document.getElementById(AtomicString("has_one_part"));
  Element* has_two_parts =
      document.getElementById(AtomicString("has_two_parts"));
  Element* has_no_part = document.getElementById(AtomicString("has_no_part"));

  ASSERT_TRUE(has_no_part);
  ASSERT_TRUE(has_one_part);
  ASSERT_TRUE(has_two_parts);

  {
    EXPECT_TRUE(has_one_part->HasPart());
    const DOMTokenList* part = has_one_part->GetPart();
    ASSERT_TRUE(part);
    ASSERT_EQ(1UL, part->length());
    ASSERT_EQ("partname", part->value());
  }

  {
    EXPECT_TRUE(has_two_parts->HasPart());
    const DOMTokenList* part = has_two_parts->GetPart();
    ASSERT_TRUE(part);
    ASSERT_EQ(2UL, part->length());
    ASSERT_EQ("partname1 partname2", part->value());
  }

  {
    EXPECT_FALSE(has_no_part->HasPart());
    EXPECT_FALSE(has_no_part->GetPart());

    // Calling the DOM API should force creation of an empty DOMTokenList.
    const DOMTokenList& part = has_no_part->part();
    EXPECT_FALSE(has_no_part->HasPart());
    EXPECT_EQ(&part, has_no_part->GetPart());

    // Now update the attribute value and make sure it's reflected.
    has_no_part->setAttribute(AtomicString("part"), AtomicString("partname"));
    ASSERT_EQ(1UL, part.length());
    ASSERT_EQ("partname", part.value());
  }
}

TEST_F(ElementTest, ExportpartsAttribute) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <span id='has_one_mapping' exportparts='partname1: partname2'></span>
    <span id='has_two_mappings' exportparts='partname1: partname2, partname3: partname4'></span>
    <span id='has_no_mapping'></span>
  )HTML");

  Element* has_one_mapping =
      document.getElementById(AtomicString("has_one_mapping"));
  Element* has_two_mappings =
      document.getElementById(AtomicString("has_two_mappings"));
  Element* has_no_mapping =
      document.getElementById(AtomicString("has_no_mapping"));

  ASSERT_TRUE(has_no_mapping);
  ASSERT_TRUE(has_one_mapping);
  ASSERT_TRUE(has_two_mappings);

  {
    EXPECT_TRUE(has_one_mapping->HasPartNamesMap());
    const NamesMap* part_names_map = has_one_mapping->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(1UL, part_names_map->size());
    ASSERT_EQ(
        "partname2",
        part_names_map->Get(AtomicString("partname1"))->SerializeToString());
  }

  {
    EXPECT_TRUE(has_two_mappings->HasPartNamesMap());
    const NamesMap* part_names_map = has_two_mappings->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(2UL, part_names_map->size());
    ASSERT_EQ(
        "partname2",
        part_names_map->Get(AtomicString("partname1"))->SerializeToString());
    ASSERT_EQ(
        "partname4",
        part_names_map->Get(AtomicString("partname3"))->SerializeToString());
  }

  {
    EXPECT_FALSE(has_no_mapping->HasPartNamesMap());
    EXPECT_FALSE(has_no_mapping->PartNamesMap());

    // Now update the attribute value and make sure it's reflected.
    has_no_mapping->setAttribute(AtomicString("exportparts"),
                                 AtomicString("partname1: partname2"));
    const NamesMap* part_names_map = has_no_mapping->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(1UL, part_names_map->size());
    ASSERT_EQ(
        "partname2",
        part_names_map->Get(AtomicString("partname1"))->SerializeToString());
  }
}

TEST_F(ElementTest, OptionElementDisplayNoneComputedStyle) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <optgroup id=group style='display:none'></optgroup>
    <option id=option style='display:none'></option>
    <div style='display:none'>
      <optgroup id=inner-group></optgroup>
      <option id=inner-option></option>
    </div>
  )HTML");

  EXPECT_FALSE(
      document.getElementById(AtomicString("group"))->GetComputedStyle());
  EXPECT_FALSE(
      document.getElementById(AtomicString("option"))->GetComputedStyle());
  EXPECT_FALSE(
      document.getElementById(AtomicString("inner-group"))->GetComputedStyle());
  EXPECT_FALSE(document.getElementById(AtomicString("inner-option"))
                   ->GetComputedStyle());
}

// A fake plugin which will assert that script is allowed in Destroy.
class ScriptOnDestroyPlugin : public GarbageCollected<ScriptOnDestroyPlugin>,
                              public WebPlugin {
 public:
  bool Initialize(WebPluginContainer* container) override {
    container_ = container;
    return true;
  }
  void Destroy() override {
    destroy_called_ = true;
    ASSERT_FALSE(ScriptForbiddenScope::IsScriptForbidden());
  }
  WebPluginContainer* Container() const override { return container_; }

  void UpdateAllLifecyclePhases(DocumentUpdateReason) override {}
  void Paint(cc::PaintCanvas*, const gfx::Rect&) override {}
  void UpdateGeometry(const gfx::Rect&,
                      const gfx::Rect&,
                      const gfx::Rect&,
                      bool) override {}
  void UpdateFocus(bool, mojom::blink::FocusType) override {}
  void UpdateVisibility(bool) override {}
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&,
                                       ui::Cursor*) override {
    return {};
  }
  void DidReceiveResponse(const WebURLResponse&) override {}
  void DidReceiveData(base::span<const char> data) override {}
  void DidFinishLoading() override {}
  void DidFailLoading(const WebURLError&) override {}

  void Trace(Visitor*) const {}

  bool DestroyCalled() const { return destroy_called_; }

 private:
  WebPluginContainer* container_;
  bool destroy_called_ = false;
};

TEST_F(ElementTest, CreateAndAttachShadowRootSuspendsPluginDisposal) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=target>
      <embed id=plugin type=application/x-blink-text-plugin></embed>
    </div>
  )HTML");

  // Set the plugin element up to have the ScriptOnDestroy plugin.
  auto* plugin_element = DynamicTo<HTMLPlugInElement>(
      document.getElementById(AtomicString("plugin")));
  ASSERT_TRUE(plugin_element);

  auto* plugin = MakeGarbageCollected<ScriptOnDestroyPlugin>();
  auto* plugin_container =
      MakeGarbageCollected<WebPluginContainerImpl>(*plugin_element, plugin);
  plugin->Initialize(plugin_container);
  plugin_element->SetEmbeddedContentView(plugin_container);

  // Now create a shadow root on target, which should cause the plugin to be
  // destroyed. Test passes if we pass the script forbidden check in the plugin.
  auto* target = document.getElementById(AtomicString("target"));
  target->CreateUserAgentShadowRoot();
  ASSERT_TRUE(plugin->DestroyCalled());
}

TEST_F(ElementTest, ParentComputedStyleForDocumentElement) {
  UpdateAllLifecyclePhasesForTest();

  Element* document_element = GetDocument().documentElement();
  ASSERT_TRUE(document_element);
  EXPECT_FALSE(document_element->ParentComputedStyle());
}

TEST_F(ElementTest, IsFocusableForInertInContentVisibility) {
  InsertStyleElement("div { content-visibility: auto; margin-top: -999px }");
  SetBodyContent("<div><p id='target' tabindex='-1'></p></div>");

  // IsFocusable() lays out the element to provide the correct answer.
  Element* target = GetElementById("target");
  ASSERT_EQ(target->GetLayoutObject(), nullptr);
  ASSERT_TRUE(target->IsFocusable());
  ASSERT_NE(target->GetLayoutObject(), nullptr);

  // Mark the element as inert. Due to content-visibility, the LayoutObject
  // will still think that it's not inert.
  target->SetBooleanAttribute(html_names::kInertAttr, true);
  ASSERT_FALSE(target->GetLayoutObject()->Style()->IsInert());

  // IsFocusable() should update the LayoutObject and notice that it's inert.
  ASSERT_FALSE(target->IsFocusable());
  ASSERT_TRUE(target->GetLayoutObject()->Style()->IsInert());
}

TEST_F(ElementTest, ParseFocusgroupAttrDefaultValuesWhenEmptyValue) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=not_fg></div>
    <div id=fg_empty focusgroup></div>
    <div id=fg_toolbar focusgroup="toolbar"></div>
  )HTML");

  // We use this as a "control" to validate that not all elements are treated as
  // Focusgroups.
  auto* not_fg = document.getElementById(AtomicString("not_fg"));
  ASSERT_TRUE(not_fg);

  EXPECT_EQ(
      not_fg->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  // Empty focusgroup attribute should be invalid (requires behavior token)
  auto* fg_empty = document.getElementById(AtomicString("fg_empty"));
  ASSERT_TRUE(fg_empty);

  EXPECT_EQ(
      fg_empty->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  // Toolbar behavior with default axes
  auto* fg_toolbar = document.getElementById(AtomicString("fg_toolbar"));
  ASSERT_TRUE(fg_toolbar);

  EXPECT_EQ(fg_toolbar->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));
}

TEST_F(ElementTest, ParseFocusgroupAttrSupportedAxesAreValid) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup="toolbar inline"></div>
    <div id=fg2 focusgroup="tablist block"></div>
    <div id=fg3 focusgroup="listbox">
      <div id=fg3_a focusgroup="menu inline"></div>
      <div id=fg3_b focusgroup="menubar block">
        <div id=fg3_b_1 focusgroup="radiogroup"></div>
      </div>
    </div>
  )HTML");

  // 1. Only inline should be supported.
  auto* fg1 = document.getElementById(AtomicString("fg1"));
  ASSERT_TRUE(fg1);

  EXPECT_EQ(
      fg1->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar, FocusgroupFlags::kInline));

  // 2. Only block should be supported.
  auto* fg2 = document.getElementById(AtomicString("fg2"));
  EXPECT_TRUE(fg2);

  EXPECT_EQ(
      fg2->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kTablist, FocusgroupFlags::kBlock));

  // 3. No axis specified so both should be supported
  auto* fg3 = document.getElementById(AtomicString("fg3"));
  ASSERT_TRUE(fg3);

  EXPECT_EQ(fg3->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kListbox,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  // 4. Only support inline because it's specified.
  auto* fg3_a = document.getElementById(AtomicString("fg3_a"));
  ASSERT_TRUE(fg3_a);

  EXPECT_EQ(
      fg3_a->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kMenu, FocusgroupFlags::kInline));

  // 5. Only support block because it's specified.
  auto* fg3_b = document.getElementById(AtomicString("fg3_b"));
  ASSERT_TRUE(fg3_b);

  EXPECT_EQ(
      fg3_b->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kMenubar, FocusgroupFlags::kBlock));

  // 6. Child specifying only behavior should still support both axes.
  auto* fg3_b_1 = document.getElementById(AtomicString("fg3_b_1"));
  ASSERT_TRUE(fg3_b_1);

  EXPECT_EQ(fg3_b_1->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kRadiogroup,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));
}

TEST_F(ElementTest, ParseFocusgroupAttrWrapIgnoredInDescendantsWithoutOwnWrap) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <div id=fg2 focusgroup="toolbar inline wrap"></div>
      <div id=fg3 focusgroup="toolbar block wrap"></div>
      <div id=fg4 focusgroup="toolbar wrap"></div>
    </div>
    <div id=fg5 focusgroup="toolbar inline">
      <div id=fg6 focusgroup="toolbar inline wrap"></div>
      <div id=fg7 focusgroup="toolbar block wrap"></div>
      <div id=fg8 focusgroup="toolbar wrap"></div>
    </div>
    <div id=fg9 focusgroup="toolbar block">
      <div id=fg10 focusgroup="toolbar inline wrap"></div>
      <div id=fg11 focusgroup="toolbar block wrap"></div>
      <div id=fg12 focusgroup="toolbar wrap"></div>
    </div>
  )HTML");

  auto* fg1 = document.getElementById(AtomicString("fg1"));
  auto* fg2 = document.getElementById(AtomicString("fg2"));
  auto* fg3 = document.getElementById(AtomicString("fg3"));
  auto* fg4 = document.getElementById(AtomicString("fg4"));
  auto* fg5 = document.getElementById(AtomicString("fg5"));
  auto* fg6 = document.getElementById(AtomicString("fg6"));
  auto* fg7 = document.getElementById(AtomicString("fg7"));
  auto* fg8 = document.getElementById(AtomicString("fg8"));
  auto* fg9 = document.getElementById(AtomicString("fg9"));
  auto* fg10 = document.getElementById(AtomicString("fg10"));
  auto* fg11 = document.getElementById(AtomicString("fg11"));
  auto* fg12 = document.getElementById(AtomicString("fg12"));
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(fg3);
  ASSERT_TRUE(fg4);
  ASSERT_TRUE(fg5);
  ASSERT_TRUE(fg6);
  ASSERT_TRUE(fg7);
  ASSERT_TRUE(fg8);
  ASSERT_TRUE(fg9);
  ASSERT_TRUE(fg10);
  ASSERT_TRUE(fg11);
  ASSERT_TRUE(fg12);

  // Parent supports both axes but no wrap - children should not inherit wrap
  EXPECT_EQ(fg1->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  EXPECT_EQ(
      fg2->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline));

  EXPECT_EQ(
      fg3->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock));

  EXPECT_EQ(fg4->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock |
                               FocusgroupFlags::kWrapInline |
                               FocusgroupFlags::kWrapBlock));

  // Parent supports only inline axis - children inherit this restriction
  EXPECT_EQ(
      fg5->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar, FocusgroupFlags::kInline));

  EXPECT_EQ(
      fg6->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline));

  EXPECT_EQ(
      fg7->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock));

  EXPECT_EQ(fg8->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock |
                               FocusgroupFlags::kWrapInline |
                               FocusgroupFlags::kWrapBlock));

  // Parent supports only block axis - children inherit this restriction
  EXPECT_EQ(
      fg9->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar, FocusgroupFlags::kBlock));

  EXPECT_EQ(
      fg10->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline));

  EXPECT_EQ(
      fg11->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kToolbar,
                     FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock));

  EXPECT_EQ(fg12->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock |
                               FocusgroupFlags::kWrapInline |
                               FocusgroupFlags::kWrapBlock));
}

TEST_F(ElementTest, ParseFocusgroupAttrGrid) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <!-- Not an error, since an author might provide the table structure in CSS. -->
    <div id=e1 focusgroup=grid></div>
    <table id=e2 focusgroup=grid></table>
    <table id=e3 focusgroup="grid wrap"></table>
    <table id=e4 focusgroup="grid row-wrap"></table>
    <table id=e5 focusgroup="grid col-wrap"></table>
    <table id=e6 focusgroup="grid row-wrap col-wrap"></table>
    <table id=e7 focusgroup="grid flow"></table>
    <table id=e8 focusgroup="grid row-flow"></table>
    <table id=e9 focusgroup="grid col-flow"></table>
    <table id=e10 focusgroup="grid row-flow col-flow"></table>
    <table id=e11 focusgroup="grid row-wrap row-flow"></table>
    <table id=e12 focusgroup="grid row-wrap col-flow"></table>
    <table id=e13 focusgroup="grid col-wrap col-flow"></table>
    <table id=e14 focusgroup="grid col-wrap row-flow"></table>
    <table focusgroup=grid></table>
    <div id=e15 focusgroup="flow"></div> <!-- Error -->
  )HTML");

  auto* e1 = document.getElementById(AtomicString("e1"));
  auto* e2 = document.getElementById(AtomicString("e2"));
  auto* e3 = document.getElementById(AtomicString("e3"));
  auto* e4 = document.getElementById(AtomicString("e4"));
  auto* e5 = document.getElementById(AtomicString("e5"));
  auto* e6 = document.getElementById(AtomicString("e6"));
  auto* e7 = document.getElementById(AtomicString("e7"));
  auto* e8 = document.getElementById(AtomicString("e8"));
  auto* e9 = document.getElementById(AtomicString("e9"));
  auto* e10 = document.getElementById(AtomicString("e10"));
  auto* e11 = document.getElementById(AtomicString("e11"));
  auto* e12 = document.getElementById(AtomicString("e12"));
  auto* e13 = document.getElementById(AtomicString("e13"));
  auto* e14 = document.getElementById(AtomicString("e14"));
  auto* e15 = document.getElementById(AtomicString("e15"));
  ASSERT_TRUE(e1);
  ASSERT_TRUE(e2);
  ASSERT_TRUE(e3);
  ASSERT_TRUE(e4);
  ASSERT_TRUE(e5);
  ASSERT_TRUE(e6);
  ASSERT_TRUE(e7);
  ASSERT_TRUE(e8);
  ASSERT_TRUE(e9);
  ASSERT_TRUE(e10);
  ASSERT_TRUE(e11);
  ASSERT_TRUE(e12);
  ASSERT_TRUE(e13);
  ASSERT_TRUE(e14);
  ASSERT_TRUE(e15);

  FocusgroupData e1_data = e1->GetFocusgroupData();
  FocusgroupData e2_data = e2->GetFocusgroupData();
  FocusgroupData e3_data = e3->GetFocusgroupData();
  FocusgroupData e4_data = e4->GetFocusgroupData();
  FocusgroupData e5_data = e5->GetFocusgroupData();
  FocusgroupData e6_data = e6->GetFocusgroupData();
  FocusgroupData e7_data = e7->GetFocusgroupData();
  FocusgroupData e8_data = e8->GetFocusgroupData();
  FocusgroupData e9_data = e9->GetFocusgroupData();
  FocusgroupData e10_data = e10->GetFocusgroupData();
  FocusgroupData e11_data = e11->GetFocusgroupData();
  FocusgroupData e12_data = e12->GetFocusgroupData();
  FocusgroupData e13_data = e13->GetFocusgroupData();
  FocusgroupData e14_data = e14->GetFocusgroupData();
  FocusgroupData e15_data = e15->GetFocusgroupData();

  EXPECT_EQ(e1_data,
            FocusgroupData(FocusgroupBehavior::kGrid, FocusgroupFlags::kNone));
  EXPECT_EQ(e2_data,
            FocusgroupData(FocusgroupBehavior::kGrid, FocusgroupFlags::kNone));
  EXPECT_EQ(e3_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kWrapInline |
                                        FocusgroupFlags::kWrapBlock));
  EXPECT_EQ(e4_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kWrapInline));
  EXPECT_EQ(e5_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kWrapBlock));
  EXPECT_EQ(e6_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kWrapInline |
                                        FocusgroupFlags::kWrapBlock));
  EXPECT_EQ(e7_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kRowFlow |
                                        FocusgroupFlags::kColFlow));
  EXPECT_EQ(e8_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kRowFlow));
  EXPECT_EQ(e9_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kColFlow));
  EXPECT_EQ(e10_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                     FocusgroupFlags::kRowFlow |
                                         FocusgroupFlags::kColFlow));
  // e11 has conflicting wrap/flow for row axis, so should be invalid.
  EXPECT_EQ(e11_data, FocusgroupData(FocusgroupBehavior::kNoBehavior,
                                     FocusgroupFlags::kNone));
  EXPECT_EQ(e12_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                     FocusgroupFlags::kWrapInline |
                                         FocusgroupFlags::kColFlow));
  // e13 has conflicting wrap/flow for column axis, so should be invalid.
  EXPECT_EQ(e13_data, FocusgroupData(FocusgroupBehavior::kNoBehavior,
                                     FocusgroupFlags::kNone));
  EXPECT_EQ(e14_data, FocusgroupData(FocusgroupBehavior::kGrid,
                                     FocusgroupFlags::kWrapBlock |
                                         FocusgroupFlags::kRowFlow));
  // e15 should be invalid since "flow" isn't a behavior token
  EXPECT_EQ(e15_data, FocusgroupData(FocusgroupBehavior::kNoBehavior,
                                     FocusgroupFlags::kNone));
}

TEST_F(ElementTest, ParseFocusgroupAttrOptOutNone) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=a focusgroup=none></div>
    <div id=b focusgroup="none inline"></div>
    <div id=c focusgroup="toolbar"></div>
  )HTML");

  auto* a = document.getElementById(AtomicString("a"));
  auto* b = document.getElementById(AtomicString("b"));
  auto* c = document.getElementById(AtomicString("c"));
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);

  EXPECT_EQ(a->GetFocusgroupData(), FocusgroupData(FocusgroupBehavior::kOptOut,
                                                   FocusgroupFlags::kNone));
  EXPECT_FALSE(focusgroup::IsActualFocusgroup(a->GetFocusgroupData()));

  // 'none' combined with other tokens should still opt-out (others ignored).
  EXPECT_EQ(b->GetFocusgroupData(), FocusgroupData(FocusgroupBehavior::kOptOut,
                                                   FocusgroupFlags::kNone));
  EXPECT_FALSE(focusgroup::IsActualFocusgroup(b->GetFocusgroupData()));

  EXPECT_TRUE(focusgroup::IsActualFocusgroup(c->GetFocusgroupData()));
  EXPECT_NE(c->GetFocusgroupData().behavior, FocusgroupBehavior::kNoBehavior);
}

TEST_F(ElementTest, ParseFocusgroupAttrNoMemoryToken) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=a focusgroup="toolbar no-memory"></div>
    <div id=b focusgroup="listbox inline no-memory"></div>
  )HTML");

  auto* a = document.getElementById(AtomicString("a"));
  auto* b = document.getElementById(AtomicString("b"));
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  // Default axes (inline+block) plus no-memory.
  EXPECT_EQ(a->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock |
                               FocusgroupFlags::kNoMemory));
  EXPECT_TRUE(focusgroup::IsActualFocusgroup(a->GetFocusgroupData()));

  // Explicit inline axis only + no-memory.
  EXPECT_EQ(
      b->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kListbox,
                     FocusgroupFlags::kInline | FocusgroupFlags::kNoMemory));
  EXPECT_TRUE(focusgroup::IsActualFocusgroup(b->GetFocusgroupData()));
}

TEST_F(ElementTest, ParseFocusgroupAttrValueRecomputedAfterDOMStructureChange) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup="toolbar wrap">
      <div id=fg2 focusgroup='menu inline wrap'>
      </div>
    </div>
    <div id=not-fg></div>
  )HTML");

  // 1. Validate that the |fg2| and |fg3| focusgroup properties were set
  // correctly initially.
  auto* fg2 = document.getElementById(AtomicString("fg2"));
  ASSERT_TRUE(fg2);

  EXPECT_EQ(
      fg2->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kMenu,
                     FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline));

  // 2. Move |fg2| from |fg1| to |not-fg|.
  auto* not_fg = document.getElementById(AtomicString("not-fg"));
  ASSERT_TRUE(not_fg);

  not_fg->AppendChild(fg2);

  // 3. Validate that the focusgroup properties were updated correctly on |fg2|
  // after they moved to a different ancestor. (No change)
  EXPECT_EQ(
      fg2->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kMenu,
                     FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline));
}

TEST_F(ElementTest, ParseFocusgroupAttrValueClearedAfterNodeRemoved) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <div id=fg2 focusgroup="menu"></div>
    </div>
  )HTML");

  // 1. Validate that the |fg1| and |fg1| focusgroup properties were set
  // correctly initially.
  auto* fg1 = document.getElementById(AtomicString("fg1"));
  ASSERT_TRUE(fg1);

  EXPECT_NE(fg1->GetFocusgroupData().behavior, FocusgroupBehavior::kNoBehavior);

  auto* fg2 = document.getElementById(AtomicString("fg2"));
  ASSERT_TRUE(fg2);

  EXPECT_NE(fg2->GetFocusgroupData().behavior, FocusgroupBehavior::kNoBehavior);

  // 2. Remove |fg1| from the DOM.
  fg1->remove();

  // 3. Validate that the focusgroup properties were cleared from both
  // focusgroups.
  EXPECT_EQ(
      fg1->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  EXPECT_EQ(
      fg2->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));
}

TEST_F(ElementTest, FocusgroupFlagsToString) {
  // Only test flag combinations that the parser can currently produce.
  FocusgroupData none_data{FocusgroupBehavior::kNoBehavior,
                           FocusgroupFlags::kNone};
  EXPECT_EQ("No behavior",
            focusgroup::FocusgroupDataToStringForTesting(none_data));

  // Behavior tokens - toolbar with inline axis.
  FocusgroupData toolbar_inline_data{FocusgroupBehavior::kToolbar,
                                     FocusgroupFlags::kInline};
  EXPECT_EQ("toolbar:(inline)",
            focusgroup::FocusgroupDataToStringForTesting(toolbar_inline_data));

  // Behavior tokens - tablist with inline axis.
  FocusgroupData tablist_inline_data{FocusgroupBehavior::kTablist,
                                     FocusgroupFlags::kInline};
  EXPECT_EQ("tablist:(inline)",
            focusgroup::FocusgroupDataToStringForTesting(tablist_inline_data));

  // Behavior tokens - listbox with block axis.
  FocusgroupData listbox_block_data{FocusgroupBehavior::kListbox,
                                    FocusgroupFlags::kBlock};
  EXPECT_EQ("listbox:(block)",
            focusgroup::FocusgroupDataToStringForTesting(listbox_block_data));

  // Behavior tokens - radiogroup with block axis.
  FocusgroupData radiogroup_block_data{FocusgroupBehavior::kRadiogroup,
                                       FocusgroupFlags::kBlock};
  EXPECT_EQ("radiogroup:(block)", focusgroup::FocusgroupDataToStringForTesting(
                                      radiogroup_block_data));

  // Linear focusgroup with wrap inline only.
  FocusgroupData toolbar_wrap_inline_data{
      FocusgroupBehavior::kToolbar,
      static_cast<FocusgroupFlags>(FocusgroupFlags::kInline |
                                   FocusgroupFlags::kWrapInline)};
  EXPECT_EQ(
      "toolbar:(inline|wrap|row-wrap)",
      focusgroup::FocusgroupDataToStringForTesting(toolbar_wrap_inline_data));

  // Grid basic.
  FocusgroupData grid_basic_data{FocusgroupBehavior::kGrid,
                                 FocusgroupFlags::kNone};
  EXPECT_EQ("grid:()",
            focusgroup::FocusgroupDataToStringForTesting(grid_basic_data));

  // Grid with row wrap only.
  FocusgroupData grid_wrap_inline_data{FocusgroupBehavior::kGrid,
                                       FocusgroupFlags::kWrapInline};
  EXPECT_EQ(
      "grid:(wrap|row-wrap)",
      focusgroup::FocusgroupDataToStringForTesting(grid_wrap_inline_data));

  // Grid with row flow only.
  FocusgroupData grid_row_flow_data{FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kRowFlow};
  EXPECT_EQ("grid:(flow|row-flow)",
            focusgroup::FocusgroupDataToStringForTesting(grid_row_flow_data));

  // Grid with column flow only.
  FocusgroupData grid_col_flow_data{FocusgroupBehavior::kGrid,
                                    FocusgroupFlags::kColFlow};
  EXPECT_EQ("grid:(flow|col-flow)",
            focusgroup::FocusgroupDataToStringForTesting(grid_col_flow_data));

  // Opt-out.
  FocusgroupData opt_out_data{FocusgroupBehavior::kOptOut,
                              FocusgroupFlags::kNone};
  EXPECT_EQ("none:()",
            focusgroup::FocusgroupDataToStringForTesting(opt_out_data));

  // No-memory modifier with block axis.
  FocusgroupData toolbar_no_memory_data{
      FocusgroupBehavior::kToolbar,
      static_cast<FocusgroupFlags>(FocusgroupFlags::kBlock |
                                   FocusgroupFlags::kNoMemory)};
  EXPECT_EQ(
      "toolbar:(block|no-memory)",
      focusgroup::FocusgroupDataToStringForTesting(toolbar_no_memory_data));
}

TEST_F(ElementTest, FocusgroupMinimumAriaRole) {
  // Test mapping from focusgroup behavior flags to ARIA roles.
  // Note: FocusgroupMinimumAriaRole requires valid focusgroup flags
  // (kNone and kOptOut are invalid and will cause a CHECK failure)

  // Behavior flags should map to corresponding ARIA roles
  EXPECT_EQ(ax::mojom::blink::Role::kToolbar,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kToolbar, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kTabList,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kTablist, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kRadioGroup,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kRadiogroup, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kListBox,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kListbox, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kMenu,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kMenu, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kMenuBar,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kMenubar, FocusgroupFlags::kNone}));
  EXPECT_EQ(ax::mojom::blink::Role::kGrid,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kGrid, FocusgroupFlags::kNone}));

  // Behavior flags combined with navigation modifiers should still return
  // the correct role (navigation modifiers don't change the base role)
  EXPECT_EQ(ax::mojom::blink::Role::kToolbar,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kToolbar, FocusgroupFlags::kInline}));

  EXPECT_EQ(ax::mojom::blink::Role::kGrid,
            focusgroup::FocusgroupMinimumAriaRole(
                {FocusgroupBehavior::kGrid, FocusgroupFlags::kWrapInline}));
}

TEST_F(ElementTest, MixStyleAttributeAndCSSOMChanges) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id="elmt" style="color: green;"></div>
  )HTML");

  Element* elmt = document.getElementById(AtomicString("elmt"));
  elmt->style()->setProperty(GetDocument().GetExecutionContext(), "color",
                             "red", String(), ASSERT_NO_EXCEPTION);

  // Verify that setting the style attribute back to its initial value is not
  // mistakenly considered as a no-op attribute change and ignored. It would be
  // without proper synchronization of attributes.
  elmt->setAttribute(html_names::kStyleAttr, AtomicString("color: green;"));

  EXPECT_EQ(elmt->getAttribute(html_names::kStyleAttr), "color: green;");
  EXPECT_EQ(elmt->style()->getPropertyValue("color"), "green");
}

TEST_F(ElementTest, GetPseudoElement) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #before::before { content:"a"; }
    #after::after { content:"a"; }
    #marker1 { display: list-item; }
    #marker2 { display: flow-root list-item; }
    #marker3 { display: inline flow list-item; }
    #marker4 { display: inline flow-root list-item; }
    </style>
    <div id="before"></div>
    <div id="after">flow</div>
    <div id="marker1"></div>
    <div id="marker2"></div>
    <div id="marker3"></div>
    <div id="marker4"></div>
    )HTML");
  // GetPseudoElement() relies on style recalc.
  GetDocument().UpdateStyleAndLayoutTree();
  struct {
    const char* id_name;
    bool has_before;
    bool has_after;
    bool has_marker;
  } kExpectations[] = {
      {"before", true, false, false},  {"after", false, true, false},
      {"marker1", false, false, true}, {"marker2", false, false, true},
      {"marker3", false, false, true}, {"marker4", false, false, true},
  };
  for (const auto& e : kExpectations) {
    SCOPED_TRACE(e.id_name);
    Element* element = GetElementById(e.id_name);
    EXPECT_EQ(e.has_before, !!element->GetPseudoElement(kPseudoIdBefore));
    EXPECT_EQ(e.has_after, !!element->GetPseudoElement(kPseudoIdAfter));
    EXPECT_EQ(e.has_marker, !!element->GetPseudoElement(kPseudoIdMarker));
  }
}

TEST_F(ElementTest, ColumnPseudoElements) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style id="test-style">
    #test::column { content: "*"; opacity: 0.5; }
    #test::column::scroll-marker { content: "+"; opacity: 0.3; }
    </style>
    <div id="test"></div>
    )HTML");
  // GetPseudoElement() relies on style recalc.
  GetDocument().UpdateStyleAndLayoutTree();

  Element* element = GetElementById("test");

  PhysicalRect dummy_column_rect;
  PseudoElement* first_column_pseudo_element =
      element->GetOrCreateColumnPseudoElementIfNeeded(0u, dummy_column_rect);
  ASSERT_TRUE(first_column_pseudo_element);
  EXPECT_EQ(first_column_pseudo_element->GetComputedStyle()->Opacity(), 0.5f);
  ASSERT_TRUE(
      first_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker));
  EXPECT_EQ(first_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker)
                ->GetComputedStyle()
                ->Opacity(),
            0.3f);

  PseudoElement* second_column_pseudo_element =
      element->GetOrCreateColumnPseudoElementIfNeeded(1u, dummy_column_rect);
  ASSERT_TRUE(second_column_pseudo_element);
  EXPECT_EQ(second_column_pseudo_element->GetComputedStyle()->Opacity(), 0.5f);
  ASSERT_TRUE(
      second_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker));
  EXPECT_EQ(
      second_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker)
          ->GetComputedStyle()
          ->Opacity(),
      0.3f);

  PseudoElement* third_column_pseudo_element =
      element->GetOrCreateColumnPseudoElementIfNeeded(2u, dummy_column_rect);
  ASSERT_TRUE(third_column_pseudo_element);
  EXPECT_EQ(third_column_pseudo_element->GetComputedStyle()->Opacity(), 0.5f);
  ASSERT_TRUE(
      third_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker));
  EXPECT_EQ(third_column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker)
                ->GetComputedStyle()
                ->Opacity(),
            0.3f);

  ASSERT_TRUE(element->GetColumnPseudoElements());
  EXPECT_EQ(element->GetColumnPseudoElements()->size(), 3u);

  Element* style = GetElementById("test-style");
  style->SetInnerHTMLWithoutTrustedTypes("");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_EQ(element->GetColumnPseudoElements()->size(), 0u);
}

TEST_F(ElementTest, TheCheckMarkPseudoElement) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .checked::checkmark {
        content: "*";
      }

      .base-button {
        appearance: base-select;
      }

      .base-picker::picker(select) {
        appearance: base-select;
      }
    </style>

    <div class="checked" id="a-div"></div>

    <select class="checked">
      <option id="not-base-option" value="the only option"></option>
    </select>

    <select class="checked base-button">
      <option id="base-button-option" value="the only option"></option>
    </select>

    <select class="checked base-picker">
      <option id="base-picker-option" value="the only option"></option>
    </select>

    <select class="checked base-picker base-button" id="target">
      <option id="target-option" value="the only option"></option>
    </select>
    )HTML");

  // GetPseudoElement() relies on style recalc.
  GetDocument().UpdateStyleAndLayoutTree();

  auto checkmark_pseudo_for = [this](const char* id) -> PseudoElement* {
    Element* e = GetElementById(id);
    return e->GetPseudoElement(kPseudoIdCheckMark);
  };

  // The `::checkmark` pseudo-element should only be created for option
  // elements in an appearance:base-select.
  EXPECT_EQ(nullptr, checkmark_pseudo_for("a-div"));
  EXPECT_EQ(nullptr, checkmark_pseudo_for("not-base-option"));
  EXPECT_EQ(nullptr, checkmark_pseudo_for("base-button-option"));
  EXPECT_EQ(nullptr, checkmark_pseudo_for("base-picker-option"));
  EXPECT_EQ(nullptr, checkmark_pseudo_for("target"));
  EXPECT_EQ(nullptr, checkmark_pseudo_for("target-option"));

  LocalFrame::NotifyUserActivation(
      GetDocument().GetFrame(), mojom::UserActivationNotificationType::kTest);
  To<HTMLSelectElement>(GetElementById("target"))
      ->showPicker(ASSERT_NO_EXCEPTION);
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_EQ(nullptr, checkmark_pseudo_for("target"));
  EXPECT_NE(nullptr, checkmark_pseudo_for("target-option"));
}

TEST_F(ElementTest, ThePickerIconPseudoElement) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #a-div::picker-icon {
        content: "*";
      }

      #target::picker-icon {
        content: "*";
      }
    </style>

    <div id="a-div"></div>

    <select id="target">
      <option id="target-option" value="the only option"></option>
    </select>
    )HTML");

  // GetPseudoElement() relies on style recalc.
  GetDocument().UpdateStyleAndLayoutTree();

  Element* div = GetElementById("a-div");
  EXPECT_EQ(nullptr, div->GetPseudoElement(kPseudoIdPickerIcon));

  // The `::picker-icon` pseudo-element should only be created for select
  // elements.
  Element* target = GetElementById("target");
  EXPECT_NE(nullptr, target->GetPseudoElement(kPseudoIdPickerIcon));

  Element* target_option = GetElementById("target-option");
  EXPECT_EQ(nullptr, target_option->GetPseudoElement(kPseudoIdPickerIcon));
}

TEST_F(ElementTest, OverscrollPseudoElementLayoutStructure) {
  ScopedCSSOverscrollGesturesForTest enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      div, #scroller::before {
        /* Prevent wrapping by anonymous blocks. */
        display: block;
      }
      #scroller {
        overscroll-area: --foo, --bar;
      }
      #scroller::before {
        content: "::before pseudo";
      }
    </style>
    <div id="previous-sibling"></div>
    <div id="scroller">
      <div id="child"></div>
    </div>
    <div id="next-sibling"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* scroller = GetElementById("scroller");
  const OverscrollAreaParentPseudoElementsVector* overscroll_elements =
      scroller->GetOverscrollAreaParentPseudoElements();
  PseudoElement* overscroll_parent_foo = overscroll_elements->at(0);
  PseudoElement* overscroll_parent_bar = overscroll_elements->at(1);

  ASSERT_TRUE(overscroll_parent_foo);
  ASSERT_TRUE(overscroll_parent_bar);
  EXPECT_FALSE(scroller->GetPseudoElement(kPseudoIdOverscrollAreaParent,
                                          AtomicString("--baz")));

  // Order of children and pseudos within content:
  EXPECT_EQ(scroller->GetPseudoElement(kPseudoIdBefore)
                ->GetLayoutObject()
                ->PreviousSibling(),
            overscroll_parent_bar->GetLayoutObject());
  EXPECT_EQ(GetElementById("child")->GetLayoutObject()->PreviousSibling(),
            scroller->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());

  // Overscroll area parents:
  EXPECT_EQ(overscroll_parent_bar->GetLayoutObject()->Parent(),
            scroller->GetLayoutObject());
  EXPECT_EQ(overscroll_parent_foo->GetLayoutObject()->Parent(),
            scroller->GetLayoutObject());

  // Scroller siblings:
  EXPECT_EQ(scroller->GetLayoutObject()->PreviousSibling(),
            GetElementById("previous-sibling")->GetLayoutObject());
  EXPECT_EQ(scroller->GetLayoutObject()->NextSibling(),
            GetElementById("next-sibling")->GetLayoutObject());
}

TEST_F(ElementTest, OverscrollPropertyTrees) {
  ScopedCSSOverscrollGesturesForTest enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container {
        overscroll-area: --foo, --bar;
        overflow: auto;
      }
    </style>
    <div id="container"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* container = GetElementById("container");
  const OverscrollAreaParentPseudoElementsVector* overscroll_elements =
      container->GetOverscrollAreaParentPseudoElements();
  PseudoElement* foo = overscroll_elements->at(0);
  PseudoElement* bar = overscroll_elements->at(1);

  // ::-internal-overscroll-area-parent skips the scrollers scroll translation.
  for (auto* pseudo_element : {foo, bar}) {
    EXPECT_EQ(pseudo_element->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation()
                  ->Parent(),
              container->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation());
  }

  // Scroll chains from the element, to the overscroll-area-parents, to the
  // root.
  HeapVector<Member<const ScrollPaintPropertyNode>> scroll_chain(
      {container->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->Scroll(),
       bar->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       foo->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       GetDocument().View()->GetPage()->GetVisualViewport().GetScrollNode()});
  for (size_t i = 1; i < scroll_chain.size(); ++i) {
    const ScrollPaintPropertyNode* child = scroll_chain[i - 1];
    const ScrollPaintPropertyNode* parent = scroll_chain[i];
    EXPECT_EQ(child->Parent(), parent);
  }
}

TEST_F(ElementTest, OverscrollPseudoElementStyles) {
  ScopedCSSOverscrollGesturesForTest enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #scroller, #non-scroller {
        overscroll-area: --foo;
      }
      #scroller {
        overflow: auto;
      }
      /* Only UA stylesheets should be able to style these pseudo-elements.
       * The following styles SHOULD NOT apply. */
      #scroller::-internal-overscroll-area-parent(*),
      #non-scroller::-internal-overscroll-area-parent(*) {
        backface-visibility: hidden;
      }
    </style>
    <div id="scroller"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* scroller = GetElementById("scroller");
  PseudoElement* overscroll_parent_foo =
      scroller->GetOverscrollAreaParentPseudoElements()->at(0);

  ASSERT_TRUE(overscroll_parent_foo);

  // Computed style of the overscroll area parent pseudo-elements
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowY());
  EXPECT_EQ(EScrollbarWidth::kNone,
            overscroll_parent_foo->GetComputedStyle()->ScrollbarWidth());

  // Computed style of the overscroll area parent pseudo-elements
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowY());
  EXPECT_EQ(EScrollbarWidth::kNone,
            overscroll_parent_foo->GetComputedStyle()->ScrollbarWidth());

  // Only UA selectors can match these pseudo-elements,
  // backface-visibility should be unchanged.
  EXPECT_EQ(EBackfaceVisibility::kVisible,
            overscroll_parent_foo->GetComputedStyle()->BackfaceVisibility());
}

// TODO(crbug.com/463729080): Enable this when the layout objects are properly
// created.
TEST_F(ElementTest, DISABLED_OverscrollContainerWithElement) {
  ScopedCSSOverscrollGesturesForTest enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="container" overscrollcontainer>
      <div id="menu"></div>
      <div id="content"></div>
    </div>
    <button id=button command="toggle-overscroll" commandfor="menu"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  ASSERT_TRUE(container);
  PseudoElement* overscroll_area_parent =
      container->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  Element* menu = GetElementById("menu");
  Element* content = GetElementById("content");
  ASSERT_TRUE(overscroll_area_parent);
  ASSERT_TRUE(menu);
  ASSERT_TRUE(content);

  // We expect the following layout tree:
  // container
  //   overscroll-area-parent
  //     menu
  //   content
  EXPECT_EQ(menu->GetLayoutObject()->Parent(),
            overscroll_area_parent->GetLayoutObject());
  EXPECT_EQ(overscroll_area_parent->GetLayoutObject(),
            container->GetLayoutObject());
  EXPECT_EQ(content->GetLayoutObject()->Parent(), container->GetLayoutObject());
}

TEST_F(ElementTest, GenerateScrollMarkerGroup) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style id="test-style">
      #scroller {
        scroll-marker-group: before;
        overflow: scroll;
      }
      #non-scroller {
        scroll-marker-group: before;
      }
    </style>
    <div id="scroller"></div>
    <div id="non-scroller"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* scroller = GetElementById("scroller");
  Element* non_scroller = GetElementById("non-scroller");

  EXPECT_TRUE(scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore));
  EXPECT_FALSE(
      non_scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore));
}

TEST_F(ElementTest, NestedMarkerInheritsFromPseudoParent) {
  ScopedCSSNestedPseudoElementsForTest feature(false);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    li {
      list-style-type: none;
    }

    li::before {
      content: '';
      list-style-type: disc;
      list-style-position: inside;
      float: left;
      display: list-item;
    }
    </style>
    <ul>
      <li id="target">Item 1</li>
    </ul>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  Element* before = target->GetPseudoElement(kPseudoIdBefore);
  Element* marker = before->GetPseudoElement(kPseudoIdMarker);

  EXPECT_EQ(marker->GetComputedStyle()->ListStyleType()->GetCounterStyleName(),
            AtomicString("disc"));
}

TEST_F(ElementTest, ScrollIntoViewNearestUseCounted) {
  // Set via setAttribute
  SetBodyInnerHTML(R"HTML(
    <body>
      <div id=target></div>
    </body>
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kScrollIntoViewContainerNearest));
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setContainer(V8ScrollContainer::Enum::kNearest);
  GetElementById("target")->scrollIntoViewWithOptions(options);
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kScrollIntoViewContainerNearest));
}

TEST_F(ElementTest, ParseFocusgroupAttrBehaviorFirstRequirement) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=invalid_empty focusgroup></div>
    <div id=invalid_inline_first focusgroup="inline toolbar"></div>
    <div id=invalid_wrap_first focusgroup="wrap menu"></div>
    <div id=valid_toolbar focusgroup="toolbar"></div>
    <div id=valid_tablist focusgroup="tablist inline"></div>
    <div id=valid_radiogroup focusgroup="radiogroup block"></div>
    <div id=valid_listbox focusgroup="listbox wrap"></div>
    <div id=valid_menu focusgroup="menu"></div>
    <div id=valid_menubar focusgroup="menubar"></div>
    <div id=valid_none focusgroup="none"></div>
  )HTML");

  // Empty focusgroup should be invalid
  auto* invalid_empty = document.getElementById(AtomicString("invalid_empty"));
  ASSERT_TRUE(invalid_empty);
  EXPECT_EQ(
      invalid_empty->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  // Non-behavior token first should be invalid
  auto* invalid_inline_first =
      document.getElementById(AtomicString("invalid_inline_first"));
  ASSERT_TRUE(invalid_inline_first);
  EXPECT_EQ(
      invalid_inline_first->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  auto* invalid_wrap_first =
      document.getElementById(AtomicString("invalid_wrap_first"));
  ASSERT_TRUE(invalid_wrap_first);
  EXPECT_EQ(
      invalid_wrap_first->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kNoBehavior, FocusgroupFlags::kNone));

  // Valid behavior tokens should work
  auto* valid_toolbar = document.getElementById(AtomicString("valid_toolbar"));
  ASSERT_TRUE(valid_toolbar);
  EXPECT_EQ(valid_toolbar->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kToolbar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  auto* valid_tablist = document.getElementById(AtomicString("valid_tablist"));
  ASSERT_TRUE(valid_tablist);
  EXPECT_EQ(
      valid_tablist->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kTablist, FocusgroupFlags::kInline));

  auto* valid_radiogroup =
      document.getElementById(AtomicString("valid_radiogroup"));
  ASSERT_TRUE(valid_radiogroup);
  EXPECT_EQ(
      valid_radiogroup->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kRadiogroup, FocusgroupFlags::kBlock));

  auto* valid_listbox = document.getElementById(AtomicString("valid_listbox"));
  ASSERT_TRUE(valid_listbox);
  EXPECT_EQ(valid_listbox->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kListbox,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock |
                               FocusgroupFlags::kWrapInline |
                               FocusgroupFlags::kWrapBlock));

  auto* valid_menu = document.getElementById(AtomicString("valid_menu"));
  ASSERT_TRUE(valid_menu);
  EXPECT_EQ(valid_menu->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kMenu,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  auto* valid_menubar = document.getElementById(AtomicString("valid_menubar"));
  ASSERT_TRUE(valid_menubar);
  EXPECT_EQ(valid_menubar->GetFocusgroupData(),
            FocusgroupData(FocusgroupBehavior::kMenubar,
                           FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  auto* valid_none = document.getElementById(AtomicString("valid_none"));
  ASSERT_TRUE(valid_none);
  EXPECT_EQ(
      valid_none->GetFocusgroupData(),
      FocusgroupData(FocusgroupBehavior::kOptOut, FocusgroupFlags::kNone));
}

// Provide assertion-prettify function for gtest.
namespace focusgroup {
void PrintTo(FocusgroupFlags flags, std::ostream* os) {
  *os << FocusgroupFlagsToStringForTesting(flags).Utf8().c_str();
}
void PrintTo(FocusgroupData data, std::ostream* os) {
  *os << FocusgroupDataToStringForTesting(data).Utf8().c_str();
}
}  // namespace focusgroup

}  // namespace blink
