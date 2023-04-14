// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ElementTest : public EditingTestBase {
 private:
  ScopedFocusgroupForTest focusgroup_enabled{true};
};

TEST_F(ElementTest, SupportsFocus) {
  Document& document = GetDocument();
  DCHECK(IsA<HTMLHtmlElement>(document.documentElement()));
  document.setDesignMode("on");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(document.documentElement()->SupportsFocus())
      << "<html> with designMode=on should be focusable.";
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

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  DOMRect* bounding_client_rect = sticky->getBoundingClientRect();
  EXPECT_EQ(0, bounding_client_rect->top());
  EXPECT_EQ(25, bounding_client_rect->left());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting the bounding client rect should cause both layout and
  // compositing inputs clean to be run, and the sticky result shouldn't change.
  bounding_client_rect = sticky->getBoundingClientRect();
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

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should be offset to stay at (0, 25) relative to the
  // viewport due to the constraints.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->OffsetLeft());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting either offset should cause both layout and compositing inputs
  // clean to be run, and the sticky result shouldn't change.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(DocumentLifecycle::kLayoutClean, document.Lifecycle().GetState());

  // Dirty layout again, since |OffsetTop| will have cleaned it.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
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

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  gfx::Rect bounds_in_viewport = sticky->BoundsInWidget();
  EXPECT_EQ(0, bounds_in_viewport.y());
  EXPECT_EQ(25, bounds_in_viewport.x());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
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

  Element* a = document.getElementById("link");
  Element* img = document.getElementById("image");

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
      document.getElementById("ancestor")->GetLayoutObject();
  LayoutObject* outer_sticky =
      document.getElementById("outerSticky")->GetLayoutObject();
  LayoutObject* child = document.getElementById("child")->GetLayoutObject();
  LayoutObject* grandchild =
      document.getElementById("grandchild")->GetLayoutObject();
  LayoutObject* inner_sticky =
      document.getElementById("innerSticky")->GetLayoutObject();
  LayoutObject* great_grandchild =
      document.getElementById("greatGrandchild")->GetLayoutObject();

  EXPECT_FALSE(ancestor->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(child->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());

  // This forces 'child' to fork it's StyleRareInheritedData, so that we can
  // ensure that the sticky subtree update behavior survives forking.
  document.getElementById("child")->SetInlineStyleProperty(
      CSSPropertyID::kWebkitRubyPosition, CSSValueID::kAfter);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(DocumentLifecycle::kPaintClean, document.Lifecycle().GetState());

  EXPECT_EQ(RubyPosition::kBefore, outer_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, child->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, grandchild->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, inner_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter,
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
  document.getElementById("outerSticky")
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
  GetDocument().body()->getElementsByClassName("ABC DEF");
  GetDocument().body()->getElementsByClassName("ABC DEF");
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

  Element* rect = document.getElementById("rect");
  DOMRect* rect_bounding_client_rect = rect->getBoundingClientRect();
  EXPECT_EQ(10, rect_bounding_client_rect->left());
  EXPECT_EQ(100, rect_bounding_client_rect->top());
  EXPECT_EQ(100, rect_bounding_client_rect->width());
  EXPECT_EQ(71, rect_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), rect->BoundsInWidget());

  // TODO(pdr): Should we should be excluding the stroke (here, and below)?
  // See: https://github.com/w3c/svgwg/issues/339 and Element::ClientQuads.
  Element* stroke = document.getElementById("stroke");
  DOMRect* stroke_bounding_client_rect = stroke->getBoundingClientRect();
  EXPECT_EQ(10, stroke_bounding_client_rect->left());
  EXPECT_EQ(100, stroke_bounding_client_rect->top());
  EXPECT_EQ(100, stroke_bounding_client_rect->width());
  EXPECT_EQ(71, stroke_bounding_client_rect->height());
  // TODO(pdr): BoundsInWidget is not web exposed and should include
  // stroke.
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), stroke->BoundsInWidget());

  Element* stroke_transformed = document.getElementById("stroke_transformed");
  DOMRect* stroke_transformedbounding_client_rect =
      stroke_transformed->getBoundingClientRect();
  EXPECT_EQ(13, stroke_transformedbounding_client_rect->left());
  EXPECT_EQ(105, stroke_transformedbounding_client_rect->top());
  EXPECT_EQ(100, stroke_transformedbounding_client_rect->width());
  EXPECT_EQ(71, stroke_transformedbounding_client_rect->height());
  // TODO(pdr): BoundsInWidget is not web exposed and should include
  // stroke.
  EXPECT_EQ(gfx::Rect(13, 105, 100, 71), stroke_transformed->BoundsInWidget());

  Element* foreign = document.getElementById("foreign");
  DOMRect* foreign_bounding_client_rect = foreign->getBoundingClientRect();
  EXPECT_EQ(10, foreign_bounding_client_rect->left());
  EXPECT_EQ(100, foreign_bounding_client_rect->top());
  EXPECT_EQ(100, foreign_bounding_client_rect->width());
  EXPECT_EQ(71, foreign_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), foreign->BoundsInWidget());

  Element* foreign_transformed = document.getElementById("foreign_transformed");
  DOMRect* foreign_transformed_bounding_client_rect =
      foreign_transformed->getBoundingClientRect();
  EXPECT_EQ(13, foreign_transformed_bounding_client_rect->left());
  EXPECT_EQ(105, foreign_transformed_bounding_client_rect->top());
  EXPECT_EQ(100, foreign_transformed_bounding_client_rect->width());
  EXPECT_EQ(71, foreign_transformed_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(13, 105, 100, 71), foreign_transformed->BoundsInWidget());

  Element* svg = document.getElementById("svg");
  DOMRect* svg_bounding_client_rect = svg->getBoundingClientRect();
  EXPECT_EQ(10, svg_bounding_client_rect->left());
  EXPECT_EQ(100, svg_bounding_client_rect->top());
  EXPECT_EQ(100, svg_bounding_client_rect->width());
  EXPECT_EQ(71, svg_bounding_client_rect->height());
  EXPECT_EQ(gfx::Rect(10, 100, 100, 71), svg->BoundsInWidget());

  Element* svg_stroke = document.getElementById("svg_stroke");
  DOMRect* svg_stroke_bounding_client_rect =
      svg_stroke->getBoundingClientRect();
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

  Element* has_one_part = document.getElementById("has_one_part");
  Element* has_two_parts = document.getElementById("has_two_parts");
  Element* has_no_part = document.getElementById("has_no_part");

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
    has_no_part->setAttribute("part", "partname");
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

  Element* has_one_mapping = document.getElementById("has_one_mapping");
  Element* has_two_mappings = document.getElementById("has_two_mappings");
  Element* has_no_mapping = document.getElementById("has_no_mapping");

  ASSERT_TRUE(has_no_mapping);
  ASSERT_TRUE(has_one_mapping);
  ASSERT_TRUE(has_two_mappings);

  {
    EXPECT_TRUE(has_one_mapping->HasPartNamesMap());
    const NamesMap* part_names_map = has_one_mapping->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(1UL, part_names_map->size());
    ASSERT_EQ("partname2",
              part_names_map->Get("partname1").value().SerializeToString());
  }

  {
    EXPECT_TRUE(has_two_mappings->HasPartNamesMap());
    const NamesMap* part_names_map = has_two_mappings->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(2UL, part_names_map->size());
    ASSERT_EQ("partname2",
              part_names_map->Get("partname1").value().SerializeToString());
    ASSERT_EQ("partname4",
              part_names_map->Get("partname3").value().SerializeToString());
  }

  {
    EXPECT_FALSE(has_no_mapping->HasPartNamesMap());
    EXPECT_FALSE(has_no_mapping->PartNamesMap());

    // Now update the attribute value and make sure it's reflected.
    has_no_mapping->setAttribute("exportparts", "partname1: partname2");
    const NamesMap* part_names_map = has_no_mapping->PartNamesMap();
    ASSERT_TRUE(part_names_map);
    ASSERT_EQ(1UL, part_names_map->size());
    ASSERT_EQ("partname2",
              part_names_map->Get("partname1").value().SerializeToString());
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

  EXPECT_FALSE(document.getElementById("group")->GetComputedStyle());
  EXPECT_FALSE(document.getElementById("option")->GetComputedStyle());
  EXPECT_FALSE(document.getElementById("inner-group")->GetComputedStyle());
  EXPECT_FALSE(document.getElementById("inner-option")->GetComputedStyle());
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
  void DidReceiveData(const char* data, size_t data_length) override {}
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
  auto* plugin_element =
      DynamicTo<HTMLPlugInElement>(document.getElementById("plugin"));
  ASSERT_TRUE(plugin_element);

  auto* plugin = MakeGarbageCollected<ScriptOnDestroyPlugin>();
  auto* plugin_container =
      MakeGarbageCollected<WebPluginContainerImpl>(*plugin_element, plugin);
  plugin->Initialize(plugin_container);
  plugin_element->SetEmbeddedContentView(plugin_container);

  // Now create a shadow root on target, which should cause the plugin to be
  // destroyed. Test passes if we pass the script forbidden check in the plugin.
  auto* target = document.getElementById("target");
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
    <div id=fg focusgroup></div>
  )HTML");

  // We use this as a "control" to validate that not all elements are treated as
  // Focusgroups.
  auto* not_fg = document.getElementById("not_fg");
  ASSERT_TRUE(not_fg);

  FocusgroupFlags not_fg_flags = not_fg->GetFocusgroupFlags();
  ASSERT_EQ(not_fg_flags, FocusgroupFlags::kNone);

  auto* fg = document.getElementById("fg");
  ASSERT_TRUE(fg);

  FocusgroupFlags fg_flags = fg->GetFocusgroupFlags();
  ASSERT_NE(fg_flags, FocusgroupFlags::kNone);

  ASSERT_TRUE(fg_flags & FocusgroupFlags::kHorizontal);
  ASSERT_TRUE(fg_flags & FocusgroupFlags::kVertical);
  ASSERT_FALSE(fg_flags & FocusgroupFlags::kExtend);
  ASSERT_FALSE(fg_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg_flags & FocusgroupFlags::kWrapVertically);
}

TEST_F(ElementTest, ParseFocusgroupAttrSupportedAxesAreValid) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup=horizontal></div>
    <div id=fg2 focusgroup=vertical></div>
    <div id=fg3 focusgroup>
      <div id=fg3_a focusgroup="extend horizontal"></div>
      <div id=fg3_b focusgroup="extend vertical">
        <div id=fg3_b_1 focusgroup=extend></div>
      </div>
    </div>
  )HTML");

  // 1. Only horizontal should be supported.
  auto* fg1 = document.getElementById("fg1");
  ASSERT_TRUE(fg1);

  FocusgroupFlags fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_TRUE(fg1_flags & FocusgroupFlags::kHorizontal);
  ASSERT_FALSE(fg1_flags & FocusgroupFlags::kVertical);

  // 2. Only vertical should be supported.
  auto* fg2 = document.getElementById("fg2");
  ASSERT_TRUE(fg2);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kHorizontal);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kVertical);

  // 3. No axis specified so both should be supported
  auto* fg3 = document.getElementById("fg3");
  ASSERT_TRUE(fg3);

  FocusgroupFlags fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kHorizontal);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kVertical);

  // 4. Only support horizontal because it's specified, regardless of the
  // extend.
  auto* fg3_a = document.getElementById("fg3_a");
  ASSERT_TRUE(fg3_a);

  FocusgroupFlags fg3_a_flags = fg3_a->GetFocusgroupFlags();
  ASSERT_TRUE(fg3_a_flags & FocusgroupFlags::kHorizontal);
  ASSERT_FALSE(fg3_a_flags & FocusgroupFlags::kVertical);

  // 5. Only support vertical because it's specified, regardless of the extend.
  auto* fg3_b = document.getElementById("fg3_b");
  ASSERT_TRUE(fg3_b);

  FocusgroupFlags fg3_b_flags = fg3_b->GetFocusgroupFlags();
  ASSERT_FALSE(fg3_b_flags & FocusgroupFlags::kHorizontal);
  ASSERT_TRUE(fg3_b_flags & FocusgroupFlags::kVertical);

  // 6. Extends a focusgroup that only supports vertical axis, but should
  // support both axes regardless.
  auto* fg3_b_1 = document.getElementById("fg3_b_1");
  ASSERT_TRUE(fg3_b_1);

  FocusgroupFlags fg3_b_1_flags = fg3_b_1->GetFocusgroupFlags();
  ASSERT_TRUE(fg3_b_1_flags & FocusgroupFlags::kHorizontal);
  ASSERT_TRUE(fg3_b_1_flags & FocusgroupFlags::kVertical);
}

TEST_F(ElementTest, ParseFocusgroupAttrExtendCorrectly) {
  Document& document = GetDocument();
  document.body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=fg1 focusgroup>
      <div id=fg2 focusgroup=extend>
        <div>
          <div>
            <div id=fg3 focusgroup=extend></div>
          </div>
        </div>
        <div id=fg4-container>
          <template shadowroot=open>
            <div id=fg4 focusgroup=extend></div>
          </template>
        </div>
      </div>
      <div id=fg5 focusgroup></div>
    </div>
    <div id=fg6 focusgroup=extend>
  )HTML");

  // 1. Root focusgroup shouldn't extend any other.
  auto* fg1 = document.getElementById("fg1");
  ASSERT_TRUE(fg1);

  FocusgroupFlags fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_NE(fg1_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg1_flags & FocusgroupFlags::kExtend);

  // 2. Direct child on which we specified "extend" should extend.
  auto* fg2 = document.getElementById("fg2");
  ASSERT_TRUE(fg2);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kExtend);

  // 3. A focusgroup marked as extend should extend its closest ancestor even if
  // that ancestor isn't its parent.
  auto* fg3 = document.getElementById("fg3");
  ASSERT_TRUE(fg3);

  FocusgroupFlags fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_NE(fg3_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kExtend);

  // 4. A focusgroup within a ShadowDOM should be able to extend its focusgroup
  // ancestor that exists outside the ShadowDOM.
  auto* fg4_container = document.getElementById("fg4-container");
  ASSERT_TRUE(fg4_container);
  ASSERT_NE(nullptr, fg4_container->GetShadowRoot());
  auto* fg4 = fg4_container->GetShadowRoot()->getElementById("fg4");
  ASSERT_TRUE(fg4);

  FocusgroupFlags fg4_flags = fg4->GetFocusgroupFlags();
  ASSERT_NE(fg4_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg4_flags & FocusgroupFlags::kExtend);

  // 5. A focusgroup child of another focusgroup should only extend if the
  // extend keyword is specified - in this case, it's not.
  auto* fg5 = document.getElementById("fg5");
  ASSERT_TRUE(fg5);

  FocusgroupFlags fg5_flags = fg5->GetFocusgroupFlags();
  ASSERT_NE(fg5_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg5_flags & FocusgroupFlags::kExtend);

  // 6. A focusgroup that doesn't have an ancestor focusgroup can't extend.
  auto* fg6 = document.getElementById("fg6");
  ASSERT_TRUE(fg6);

  FocusgroupFlags fg6_flags = fg6->GetFocusgroupFlags();
  ASSERT_NE(fg6_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg6_flags & FocusgroupFlags::kExtend);
}

TEST_F(ElementTest, ParseFocusgroupAttrWrapCorrectly) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup=wrap>
      <div id=fg2 focusgroup=extend>
        <div id=fg3 focusgroup="extend horizontal"></div>
        <div id=fg4 focusgroup="extend vertical">
          <div id=fg5 focusgroup="extend horizontal"></div>
        </div>
      </div>
    </div>
  )HTML");

  // 1. Root focusgroup supports both axes and wraps, so should support wrapping
  // in both axes.
  auto* fg1 = document.getElementById("fg1");
  ASSERT_TRUE(fg1);

  FocusgroupFlags fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_NE(fg1_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg1_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg1_flags & FocusgroupFlags::kWrapVertically);

  // 2. When a focusgroup extends another one, it should inherit its wrap
  // properties in all supported axes.
  auto* fg2 = document.getElementById("fg2");
  ASSERT_TRUE(fg2);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kWrapVertically);

  // 3. The ancestor focusgroup's wrap properties should only be inherited in
  // the horizontal axis.
  auto* fg3 = document.getElementById("fg3");
  ASSERT_TRUE(fg3);

  FocusgroupFlags fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_NE(fg3_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg3_flags & FocusgroupFlags::kWrapVertically);

  // 4. The ancestor focusgroup's wrap properties should only be inherited in
  // the vertical axis.
  auto* fg4 = document.getElementById("fg4");
  ASSERT_TRUE(fg4);

  FocusgroupFlags fg4_flags = fg4->GetFocusgroupFlags();
  ASSERT_NE(fg4_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg4_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg4_flags & FocusgroupFlags::kWrapVertically);

  // 5. The ancestor focusgroup's wrap properties shouldn't be inherited since
  // the two focusgroups have no axis in common.
  auto* fg5 = document.getElementById("fg5");
  ASSERT_TRUE(fg5);

  FocusgroupFlags fg5_flags = fg5->GetFocusgroupFlags();
  ASSERT_NE(fg5_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg5_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg5_flags & FocusgroupFlags::kWrapVertically);
}

TEST_F(ElementTest, ParseFocusgroupAttrDoesntWrapInExtendingFocusgroupOnly) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup>
      <div id=fg2 focusgroup="extend horizontal wrap"></div>
      <div id=fg3 focusgroup="extend vertical wrap"></div>
      <div id=fg4 focusgroup="extend wrap"></div>
    </div>
    <div id=fg5 focusgroup=horizontal>
      <div id=fg6 focusgroup="extend horizontal wrap"></div>
      <div id=fg7 focusgroup="extend vertical wrap"></div>
      <div id=fg8 focusgroup="extend wrap"></div>
    </div>
    <div id=fg9 focusgroup=vertical>
      <div id=fg10 focusgroup="extend horizontal wrap"></div>
      <div id=fg11 focusgroup="extend vertical wrap"></div>
      <div id=fg12 focusgroup="extend wrap"></div>
    </div>
  )HTML");

  auto* fg1 = document.getElementById("fg1");
  auto* fg2 = document.getElementById("fg2");
  auto* fg3 = document.getElementById("fg3");
  auto* fg4 = document.getElementById("fg4");
  auto* fg5 = document.getElementById("fg5");
  auto* fg6 = document.getElementById("fg6");
  auto* fg7 = document.getElementById("fg7");
  auto* fg8 = document.getElementById("fg8");
  auto* fg9 = document.getElementById("fg9");
  auto* fg10 = document.getElementById("fg10");
  auto* fg11 = document.getElementById("fg11");
  auto* fg12 = document.getElementById("fg12");
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

  FocusgroupFlags fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_NE(fg1_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg1_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg1_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_NE(fg3_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg3_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg3_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg4_flags = fg4->GetFocusgroupFlags();
  ASSERT_NE(fg4_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg4_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg4_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg5_flags = fg5->GetFocusgroupFlags();
  ASSERT_NE(fg5_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg5_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg5_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg6_flags = fg6->GetFocusgroupFlags();
  ASSERT_NE(fg6_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg6_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg6_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg7_flags = fg7->GetFocusgroupFlags();
  ASSERT_NE(fg7_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg7_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg7_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg8_flags = fg8->GetFocusgroupFlags();
  ASSERT_NE(fg8_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg8_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg8_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg9_flags = fg9->GetFocusgroupFlags();
  ASSERT_NE(fg9_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg9_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg9_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg10_flags = fg10->GetFocusgroupFlags();
  ASSERT_NE(fg10_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg10_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg10_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg11_flags = fg11->GetFocusgroupFlags();
  ASSERT_NE(fg11_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg11_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg11_flags & FocusgroupFlags::kWrapVertically);

  FocusgroupFlags fg12_flags = fg12->GetFocusgroupFlags();
  ASSERT_NE(fg12_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg12_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg12_flags & FocusgroupFlags::kWrapVertically);
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
    <table focusgroup=grid>
      <tbody id=e15 focusgroup=extend></tbody> <!-- Error -->
    </table>
    <div id=e16 focusgroup="flow"></div> <!-- Error -->
  )HTML");

  auto* e1 = document.getElementById("e1");
  auto* e2 = document.getElementById("e2");
  auto* e3 = document.getElementById("e3");
  auto* e4 = document.getElementById("e4");
  auto* e5 = document.getElementById("e5");
  auto* e6 = document.getElementById("e6");
  auto* e7 = document.getElementById("e7");
  auto* e8 = document.getElementById("e8");
  auto* e9 = document.getElementById("e9");
  auto* e10 = document.getElementById("e10");
  auto* e11 = document.getElementById("e11");
  auto* e12 = document.getElementById("e12");
  auto* e13 = document.getElementById("e13");
  auto* e14 = document.getElementById("e14");
  auto* e15 = document.getElementById("e15");
  auto* e16 = document.getElementById("e16");
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
  ASSERT_TRUE(e16);

  FocusgroupFlags e1_flags = e1->GetFocusgroupFlags();
  FocusgroupFlags e2_flags = e2->GetFocusgroupFlags();
  FocusgroupFlags e3_flags = e3->GetFocusgroupFlags();
  FocusgroupFlags e4_flags = e4->GetFocusgroupFlags();
  FocusgroupFlags e5_flags = e5->GetFocusgroupFlags();
  FocusgroupFlags e6_flags = e6->GetFocusgroupFlags();
  FocusgroupFlags e7_flags = e7->GetFocusgroupFlags();
  FocusgroupFlags e8_flags = e8->GetFocusgroupFlags();
  FocusgroupFlags e9_flags = e9->GetFocusgroupFlags();
  FocusgroupFlags e10_flags = e10->GetFocusgroupFlags();
  FocusgroupFlags e11_flags = e11->GetFocusgroupFlags();
  FocusgroupFlags e12_flags = e12->GetFocusgroupFlags();
  FocusgroupFlags e13_flags = e13->GetFocusgroupFlags();
  FocusgroupFlags e14_flags = e14->GetFocusgroupFlags();
  FocusgroupFlags e15_flags = e15->GetFocusgroupFlags();
  FocusgroupFlags e16_flags = e16->GetFocusgroupFlags();

  ASSERT_EQ(e1_flags, FocusgroupFlags::kGrid);
  ASSERT_EQ(e2_flags, FocusgroupFlags::kGrid);
  ASSERT_EQ(e3_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapHorizontally |
             FocusgroupFlags::kWrapVertically));
  ASSERT_EQ(e4_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapHorizontally));
  ASSERT_EQ(e5_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapVertically));
  ASSERT_EQ(e6_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapHorizontally |
             FocusgroupFlags::kWrapVertically));
  ASSERT_EQ(e7_flags, (FocusgroupFlags::kGrid | FocusgroupFlags::kRowFlow |
                       FocusgroupFlags::kColFlow));
  ASSERT_EQ(e8_flags, (FocusgroupFlags::kGrid | FocusgroupFlags::kRowFlow));
  ASSERT_EQ(e9_flags, (FocusgroupFlags::kGrid | FocusgroupFlags::kColFlow));
  ASSERT_EQ(e10_flags, (FocusgroupFlags::kGrid | FocusgroupFlags::kRowFlow |
                        FocusgroupFlags::kColFlow));
  ASSERT_EQ(e11_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapHorizontally));
  ASSERT_EQ(e12_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapHorizontally |
             FocusgroupFlags::kColFlow));
  ASSERT_EQ(e13_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapVertically));
  ASSERT_EQ(e14_flags,
            (FocusgroupFlags::kGrid | FocusgroupFlags::kWrapVertically |
             FocusgroupFlags::kRowFlow));
  ASSERT_EQ(e15_flags, FocusgroupFlags::kNone);
  ASSERT_EQ(e16_flags,
            (FocusgroupFlags::kHorizontal | FocusgroupFlags::kVertical));
}

TEST_F(ElementTest, ParseFocusgroupAttrValueRecomputedAfterDOMStructureChange) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup=wrap>
      <div id=fg2 focusgroup=extend>
          <div>
            <div id=fg3 focusgroup=extend></div>
          </div>
      </div>
    </div>
    <div id=not-fg></div>
  )HTML");

  // 1. Validate that the |fg2| and |fg3| focusgroup properties were set
  // correctly initially.
  auto* fg2 = document.getElementById("fg2");
  ASSERT_TRUE(fg2);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kExtend);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kWrapVertically);

  auto* fg3 = document.getElementById("fg3");
  ASSERT_TRUE(fg3);

  FocusgroupFlags fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_NE(fg3_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kExtend);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kWrapVertically);

  // 2. Move |fg2| from |fg1| to |not-fg|.
  auto* not_fg = document.getElementById("not-fg");
  ASSERT_TRUE(not_fg);

  not_fg->AppendChild(fg2);

  // 3. Validate that the focusgroup properties were updated correctly on |fg2|
  // and |fg3| after they moved to a different ancestor.
  fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kExtend);
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg2_flags & FocusgroupFlags::kWrapVertically);

  fg3_flags = fg3->GetFocusgroupFlags();
  ASSERT_NE(fg3_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg3_flags & FocusgroupFlags::kExtend);
  ASSERT_FALSE(fg3_flags & FocusgroupFlags::kWrapHorizontally);
  ASSERT_FALSE(fg3_flags & FocusgroupFlags::kWrapVertically);
}

TEST_F(ElementTest, ParseFocusgroupAttrValueClearedAfterNodeRemoved) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id=fg1 focusgroup>
      <div id=fg2 focusgroup=extend></div>
    </div>
  )HTML");

  // 1. Validate that the |fg1| and |fg1| focusgroup properties were set
  // correctly initially.
  auto* fg1 = document.getElementById("fg1");
  ASSERT_TRUE(fg1);

  FocusgroupFlags fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_NE(fg1_flags, FocusgroupFlags::kNone);
  ASSERT_FALSE(fg1_flags & FocusgroupFlags::kExtend);

  auto* fg2 = document.getElementById("fg2");
  ASSERT_TRUE(fg2);

  FocusgroupFlags fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_NE(fg2_flags, FocusgroupFlags::kNone);
  ASSERT_TRUE(fg2_flags & FocusgroupFlags::kExtend);

  // 2. Remove |fg1| from the DOM.
  fg1->remove();

  // 3. Validate that the focusgroup properties were cleared from both
  // focusgroups.
  fg1_flags = fg1->GetFocusgroupFlags();
  ASSERT_EQ(fg1_flags, FocusgroupFlags::kNone);

  fg2_flags = fg2->GetFocusgroupFlags();
  ASSERT_EQ(fg2_flags, FocusgroupFlags::kNone);
}

TEST_F(ElementTest, MixStyleAttributeAndCSSOMChanges) {
  Document& document = GetDocument();
  SetBodyContent(R"HTML(
    <div id="elmt" style="color: green;"></div>
  )HTML");

  Element* elmt = document.getElementById("elmt");
  elmt->style()->setProperty(GetDocument().GetExecutionContext(), "color",
                             "red", String(), ASSERT_NO_EXCEPTION);

  // Verify that setting the style attribute back to its initial value is not
  // mistakenly considered as a no-op attribute change and ignored. It would be
  // without proper synchronization of attributes.
  elmt->setAttribute(html_names::kStyleAttr, "color: green;");

  EXPECT_EQ(elmt->getAttribute(html_names::kStyleAttr), "color: green;");
  EXPECT_EQ(elmt->style()->getPropertyValue("color"), "green");
}

TEST_F(ElementTest, GetPseudoElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
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

}  // namespace blink
