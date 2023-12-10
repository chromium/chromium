// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
namespace test {

namespace {

class AccessibilityObjectModelTest
    : public SimTest,
      public ScopedAccessibilityObjectModelForTest {
 public:
  AccessibilityObjectModelTest()
      : ScopedAccessibilityObjectModelForTest(true) {}

 protected:
  AXObjectCacheImpl* AXObjectCache() {
    return static_cast<AXObjectCacheImpl*>(
        GetDocument().ExistingAXObjectCache());
  }
};

TEST_F(AccessibilityObjectModelTest, DOMElementsHaveAnAccessibleNode) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* button = GetDocument().getElementById(AtomicString("button"));
  EXPECT_NE(nullptr, button->accessibleNode());
  EXPECT_TRUE(button->accessibleNode()->role().IsNull());
  EXPECT_TRUE(button->accessibleNode()->label().IsNull());
}

// AccessibleNode is being refactored to remove it's ability to modify the
// underlying accessibility tree. This test has been modified to assert that no
// changes in corresponding AXObjects are observed, but will likely be removed
// in the future.
TEST_F(AccessibilityObjectModelTest, SetAccessibleNodeRole) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);

  cache->UpdateAXForAllDocuments();

  auto* button = GetDocument().getElementById(AtomicString("button"));
  ASSERT_NE(nullptr, button);

  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(ax::mojom::Role::kButton, axButton->RoleValue());

  button->accessibleNode()->setRole(AtomicString("slider"));
  EXPECT_EQ("slider", button->accessibleNode()->role());

  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  axButton = cache->GetOrCreate(button);

  // No change in the AXObject role should be observed.
  EXPECT_EQ(ax::mojom::Role::kButton, axButton->RoleValue());
}

TEST_F(AccessibilityObjectModelTest, AOMDoesNotReflectARIA) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input id=textbox>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  // Set ARIA attributes.
  auto* textbox = GetDocument().getElementById(AtomicString("textbox"));
  ASSERT_NE(nullptr, textbox);
  textbox->setAttribute(html_names::kRoleAttr, AtomicString("combobox"));
  textbox->setAttribute(html_names::kAriaLabelAttr, AtomicString("Combo"));
  textbox->setAttribute(html_names::kAriaDisabledAttr, AtomicString("true"));

  // Assert that the ARIA attributes affect the AX object.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* axTextBox = cache->GetOrCreate(textbox);
  EXPECT_EQ(ax::mojom::Role::kTextFieldWithComboBox, axTextBox->RoleValue());
  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  EXPECT_EQ("Combo", axTextBox->GetName(name_from, &name_objects));
  EXPECT_EQ(axTextBox->Restriction(), kRestrictionDisabled);

  // The AOM properties should still all be null.
  EXPECT_EQ(nullptr, textbox->accessibleNode()->role());
  EXPECT_EQ(nullptr, textbox->accessibleNode()->label());
  EXPECT_FALSE(textbox->accessibleNode()->disabled().has_value());
}

TEST_F(AccessibilityObjectModelTest, AOMPropertiesCanBeCleared) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input type=button id=button>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  // Set ARIA attributes.
  auto* button = GetDocument().getElementById(AtomicString("button"));
  ASSERT_NE(nullptr, button);
  button->setAttribute(html_names::kRoleAttr, AtomicString("checkbox"));
  button->setAttribute(html_names::kAriaLabelAttr, AtomicString("Check"));
  button->setAttribute(html_names::kAriaDisabledAttr, AtomicString("true"));

  // Assert that the AX object was affected by ARIA attributes.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, axButton->RoleValue());
  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kRestrictionDisabled);

  // Now set the AOM properties to override.
  button->accessibleNode()->setRole(AtomicString("radio"));
  button->accessibleNode()->setLabel(AtomicString("Radio"));
  button->accessibleNode()->setDisabled(false);
  cache->UpdateAXForAllDocuments();

  // Assert that AOM does not affect the AXObject.
  axButton = cache->Get(button);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, axButton->RoleValue());
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kRestrictionDisabled);

  // Null the AOM properties.
  button->accessibleNode()->setRole(g_null_atom);
  button->accessibleNode()->setLabel(g_null_atom);
  button->accessibleNode()->setDisabled(absl::nullopt);
  cache->UpdateAXForAllDocuments();

  // The AX Object should now revert to ARIA.
  axButton = cache->Get(button);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, axButton->RoleValue());
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_EQ(axButton->Restriction(), kRestrictionDisabled);
}

TEST_F(AccessibilityObjectModelTest, RangeProperties) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=slider id=slider>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* slider = GetDocument().getElementById(AtomicString("slider"));
  ASSERT_NE(nullptr, slider);
  slider->accessibleNode()->setValueMin(-0.5);
  slider->accessibleNode()->setValueMax(0.5);
  slider->accessibleNode()->setValueNow(0.1);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* ax_slider = cache->GetOrCreate(slider);
  float value = 0.0f;
  EXPECT_TRUE(ax_slider->MinValueForRange(&value));
  EXPECT_EQ(0.0f, value);
  EXPECT_TRUE(ax_slider->MaxValueForRange(&value));
  EXPECT_EQ(100.0f, value);
  EXPECT_TRUE(ax_slider->ValueForRange(&value));
  EXPECT_EQ(50.0f, value);
}

TEST_F(AccessibilityObjectModelTest, Level) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=heading id=heading>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* heading = GetDocument().getElementById(AtomicString("heading"));
  ASSERT_NE(nullptr, heading);
  heading->accessibleNode()->setLevel(5);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* ax_heading = cache->GetOrCreate(heading);
  EXPECT_EQ(2, ax_heading->HeadingLevel());
}

TEST_F(AccessibilityObjectModelTest, ListItem) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div role=list><div role=listitem id=listitem></div></div>");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* listitem = GetDocument().getElementById(AtomicString("listitem"));
  ASSERT_NE(nullptr, listitem);
  listitem->accessibleNode()->setPosInSet(9);
  listitem->accessibleNode()->setSetSize(10);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* ax_listitem = cache->GetOrCreate(listitem);
  EXPECT_EQ(0, ax_listitem->PosInSet());
  EXPECT_EQ(0, ax_listitem->SetSize());
}

TEST_F(AccessibilityObjectModelTest, Grid) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div role=grid id=grid>
      <div role=row id=row>
        <div role=gridcell id=cell></div>
        <div role=gridcell id=cell2></div>
      </div>
    </div>
  )HTML");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* grid = GetDocument().getElementById(AtomicString("grid"));
  ASSERT_NE(nullptr, grid);
  grid->accessibleNode()->setColCount(16);
  grid->accessibleNode()->setRowCount(9);

  auto* row = GetDocument().getElementById(AtomicString("row"));
  ASSERT_NE(nullptr, row);
  row->accessibleNode()->setColIndex(8);
  row->accessibleNode()->setRowIndex(5);

  auto* cell = GetDocument().getElementById(AtomicString("cell"));

  auto* cell2 = GetDocument().getElementById(AtomicString("cell2"));
  ASSERT_NE(nullptr, cell2);
  cell2->accessibleNode()->setColIndex(10);
  cell2->accessibleNode()->setRowIndex(7);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();

  auto* ax_grid = cache->GetOrCreate(grid);
  EXPECT_EQ(0, ax_grid->AriaColumnCount());
  EXPECT_EQ(0, ax_grid->AriaRowCount());

  auto* ax_cell = cache->GetOrCreate(cell);
  EXPECT_TRUE(ax_cell->IsTableCellLikeRole());
  EXPECT_EQ(0U, ax_cell->AriaColumnIndex());
  EXPECT_EQ(0U, ax_cell->AriaRowIndex());

  auto* ax_cell2 = cache->GetOrCreate(cell2);
  EXPECT_TRUE(ax_cell2->IsTableCellLikeRole());
  EXPECT_EQ(0U, ax_cell2->AriaColumnIndex());
  EXPECT_EQ(0U, ax_cell2->AriaRowIndex());
}

TEST_F(AccessibilityObjectModelTest, SparseAttributes) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <input id=target
     aria-keyshortcuts=Ctrl+K
     aria-roledescription=Widget
     aria-virtualcontent=block-end
     aria-activedescendant=active
     aria-details=details
     aria-invalid=true
     aria-errormessage=error>
    <div id=active role=option></div>
    <div id=active2 role=gridcell></div>
    <div id=details role=contentinfo></div>
    <div id=details2 role=form></div>
    <div id=error role=article>Error</div>
    <div id=error2 role=banner>Error 2</div>
  )HTML");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  cache->UpdateAXForAllDocuments();
  auto* ax_target = cache->Get(target);
  ui::AXNodeData node_data;
  ui::AXNodeData node_data2;

  {
    cache->UpdateAXForAllDocuments();
    ScopedFreezeAXCache freeze(*cache);
    ax_target->Serialize(&node_data, ui::kAXModeComplete);

    ASSERT_EQ("Ctrl+K", node_data.GetStringAttribute(
                            ax::mojom::blink::StringAttribute::kKeyShortcuts));
    ASSERT_EQ("Widget",
              node_data.GetStringAttribute(
                  ax::mojom::blink::StringAttribute::kRoleDescription));
    ASSERT_EQ("block-end",
              node_data.GetStringAttribute(
                  ax::mojom::blink::StringAttribute::kVirtualContent));
    auto* active_descendant_target =
        cache->ObjectFromAXID(node_data.GetIntAttribute(
            ax::mojom::blink::IntAttribute::kActivedescendantId));
    ASSERT_NE(nullptr, active_descendant_target);
    ASSERT_EQ(ax::mojom::Role::kListBoxOption,
              active_descendant_target->RoleValue());
    auto* aria_details_target =
        cache->ObjectFromAXID(node_data.GetIntListAttribute(
            ax::mojom::blink::IntListAttribute::kDetailsIds)[0]);
    ASSERT_EQ(ax::mojom::Role::kContentInfo, aria_details_target->RoleValue());
    auto* error_message_target =
        cache->ObjectFromAXID(node_data.GetIntListAttribute(
            ax::mojom::blink::IntListAttribute::kErrormessageIds)[0]);
    ASSERT_NE(nullptr, error_message_target);
    ASSERT_EQ(ax::mojom::Role::kArticle, error_message_target->RoleValue());
  }

  target->accessibleNode()->setKeyShortcuts(AtomicString("Ctrl+L"));
  target->accessibleNode()->setRoleDescription(AtomicString("Object"));
  target->accessibleNode()->setVirtualContent(AtomicString("inline-start"));
  target->accessibleNode()->setActiveDescendant(
      GetDocument().getElementById(AtomicString("active2"))->accessibleNode());
  AccessibleNodeList* details_node_list =
      MakeGarbageCollected<AccessibleNodeList>();
  details_node_list->add(
      GetDocument().getElementById(AtomicString("details2"))->accessibleNode());
  target->accessibleNode()->setDetails(details_node_list);
  AccessibleNodeList* error_message_node_list =
      MakeGarbageCollected<AccessibleNodeList>();
  error_message_node_list->add(
      GetDocument().getElementById(AtomicString("error2"))->accessibleNode());
  target->accessibleNode()->setErrorMessage(error_message_node_list);

  {
    cache->UpdateAXForAllDocuments();
    ScopedFreezeAXCache freeze(*cache);
    ax_target->Serialize(&node_data2, ui::kAXModeComplete);

    ASSERT_EQ("Ctrl+K", node_data.GetStringAttribute(
                            ax::mojom::blink::StringAttribute::kKeyShortcuts));
    ASSERT_EQ("Widget",
              node_data.GetStringAttribute(
                  ax::mojom::blink::StringAttribute::kRoleDescription));
    ASSERT_EQ(target->accessibleNode()->virtualContent(), "inline-start");
    ASSERT_EQ("block-end",
              node_data.GetStringAttribute(
                  ax::mojom::blink::StringAttribute::kVirtualContent));

    auto* active_descendant_target2 =
        cache->ObjectFromAXID(node_data2.GetIntAttribute(
            ax::mojom::blink::IntAttribute::kActivedescendantId));
    ASSERT_EQ(ax::mojom::Role::kListBoxOption,
              active_descendant_target2->RoleValue());
    auto* aria_details_target2 =
        cache->ObjectFromAXID(node_data2.GetIntListAttribute(
            ax::mojom::blink::IntListAttribute::kDetailsIds)[0]);
    ASSERT_EQ(ax::mojom::Role::kContentInfo, aria_details_target2->RoleValue());
    auto* error_message_target2 =
        cache->ObjectFromAXID(node_data2.GetIntListAttribute(
            ax::mojom::blink::IntListAttribute::kErrormessageIds)[0]);
    ASSERT_NE(nullptr, error_message_target2);
    ASSERT_EQ(ax::mojom::Role::kArticle, error_message_target2->RoleValue());
  }
}

TEST_F(AccessibilityObjectModelTest, LabeledBy) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <input id=target aria-labelledby='l1 l2'>
    <label id=l1>Label 1</label>
    <label id=l2>Label 2</label>
    <label id=l3>Label 3</label>
  )HTML");
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* l1 = GetDocument().getElementById(AtomicString("l1"));
  auto* l2 = GetDocument().getElementById(AtomicString("l2"));
  auto* l3 = GetDocument().getElementById(AtomicString("l3"));

  HeapVector<Member<Element>> labeled_by;
  ASSERT_TRUE(AccessibleNode::GetPropertyOrARIAAttribute(
      target, AOMRelationListProperty::kLabeledBy, labeled_by));
  ASSERT_EQ(2U, labeled_by.size());
  ASSERT_EQ(l1, labeled_by[0]);
  ASSERT_EQ(l2, labeled_by[1]);

  AccessibleNodeList* node_list = target->accessibleNode()->labeledBy();
  ASSERT_EQ(nullptr, node_list);

  node_list = MakeGarbageCollected<AccessibleNodeList>();
  node_list->add(l3->accessibleNode());
  target->accessibleNode()->setLabeledBy(node_list);

  labeled_by.clear();
  ASSERT_TRUE(AccessibleNode::GetPropertyOrARIAAttribute(
      target, AOMRelationListProperty::kLabeledBy, labeled_by));
  ASSERT_EQ(2U, labeled_by.size());
}

}  // namespace

}  // namespace test
}  // namespace blink
