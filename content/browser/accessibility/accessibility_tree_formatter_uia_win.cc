// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_uia_win.h"

#include <math.h>
#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/gfx/win/hwnd_util.h"

namespace {

std::string UiaIdentifierToCondensedString(int32_t id) {
  std::string identifier = content::UiaIdentifierToStringUTF8(id);
  if (id >= UIA_RuntimeIdPropertyId && id <= UIA_HeadingLevelPropertyId) {
    // remove leading 'UIA_' and trailing 'PropertyId'
    return identifier.substr(4, identifier.size() - 14);
  }
  if (id >= UIA_ButtonControlTypeId && id <= UIA_AppBarControlTypeId) {
    // remove leading 'UIA_' and trailing 'ControlTypeId'
    return identifier.substr(4, identifier.size() - 17);
  }
  return identifier;
}

}  // namespace

namespace content {

// This is the list of interesting properties to dump.
//
// Certain properties are skipped because they are known to cause crashes if the
// underlying pattern isn't implemented (e.g., LegacyIAccessibleSelection will
// crash on Win7 if the LegacyIAccessible pattern isn't implemented).
//
// Other properties aren't interesting in a tree-dump context (e.g., ProcessId).
//
// Finally, certain properties are dumped as part of a pattern, and don't need
// to be dumped a second time here (e.g., Grid*, GridItem*, RangeValue*, etc.).

// static
const long AccessibilityTreeFormatterUia::properties_[] = {
    // UIA_RuntimeIdPropertyId                          // 30000
    UIA_BoundingRectanglePropertyId,                    // 30001
    // UIA_ProcessIdPropertyId                          // 30002
    UIA_ControlTypePropertyId,                          // 30003
    UIA_LocalizedControlTypePropertyId,                 // 30004
    UIA_NamePropertyId,                                 // 30005
    UIA_AcceleratorKeyPropertyId,                       // 30006
    UIA_AccessKeyPropertyId,                            // 30007
    UIA_HasKeyboardFocusPropertyId,                     // 30008
    UIA_IsKeyboardFocusablePropertyId,                  // 30009
    UIA_IsEnabledPropertyId,                            // 30010
    UIA_AutomationIdPropertyId,                         // 30011
    UIA_ClassNamePropertyId,                            // 30012
    UIA_HelpTextPropertyId,                             // 30013
    UIA_ClickablePointPropertyId,                       // 30014
    UIA_CulturePropertyId,                              // 30015
    UIA_IsControlElementPropertyId,                     // 30016
    UIA_IsContentElementPropertyId,                     // 30017
    UIA_LabeledByPropertyId,                            // 30018
    UIA_IsPasswordPropertyId,                           // 30019
    // UIA_NativeWindowHandlePropertyId                 // 30020
    UIA_ItemTypePropertyId,                             // 30021
    UIA_IsOffscreenPropertyId,                          // 30022
    UIA_OrientationPropertyId,                          // 30023
    UIA_FrameworkIdPropertyId,                          // 30024
    UIA_IsRequiredForFormPropertyId,                    // 30025
    UIA_ItemStatusPropertyId,                           // 30026
    UIA_IsDockPatternAvailablePropertyId,               // 30027
    UIA_IsExpandCollapsePatternAvailablePropertyId,     // 30028
    UIA_IsGridItemPatternAvailablePropertyId,           // 30029
    UIA_IsGridPatternAvailablePropertyId,               // 30030
    UIA_IsInvokePatternAvailablePropertyId,             // 30031
    UIA_IsMultipleViewPatternAvailablePropertyId,       // 30032
    UIA_IsRangeValuePatternAvailablePropertyId,         // 30033
    UIA_IsScrollPatternAvailablePropertyId,             // 30034
    UIA_IsScrollItemPatternAvailablePropertyId,         // 30035
    UIA_IsSelectionItemPatternAvailablePropertyId,      // 30036
    UIA_IsSelectionPatternAvailablePropertyId,          // 30037
    UIA_IsTablePatternAvailablePropertyId,              // 30038
    UIA_IsTableItemPatternAvailablePropertyId,          // 30039
    UIA_IsTextPatternAvailablePropertyId,               // 30040
    UIA_IsTogglePatternAvailablePropertyId,             // 30041
    UIA_IsTransformPatternAvailablePropertyId,          // 30042
    UIA_IsValuePatternAvailablePropertyId,              // 30043
    UIA_IsWindowPatternAvailablePropertyId,             // 30044
    // UIA_Value*                                       // 30045-30046
    // UIA_RangeValue*                                  // 30047-30052
    // UIA_Scroll*                                      // 30053-30058
    // UIA_Selection*                                   // 30059-30061
    // UIA_Grid*                                        // 30062-30068
    // UIA_DockDockPositionPropertyId,                  // 30069
    // UIA_ExpandCollapseExpandCollapseStatePropertyId, // 30070
    // UIA_MultipleViewCurrentViewPropertyId,           // 30071
    // UIA_MultipleViewSupportedViewsPropertyId,        // 30072
    // UIA_WindowCanMaximizePropertyId,                 // 30073
    // UIA_WindowCanMinimizePropertyId,                 // 30074
    // UIA_WindowWindowVisualStatePropertyId,           // 30075
    // UIA_WindowWindowInteractionStatePropertyId,      // 30076
    // UIA_WindowIsModalPropertyId                      // 30077
    // UIA_WindowIsTopmostPropertyId,                   // 30078
    // UIA_SelectionItem*                               // 30079-30080
    // UIA_TableRowHeadersPropertyId,                   // 30081
    // UIA_TableColumnHeadersPropertyId,                // 30082
    // UIA_TableRowOrColumnMajorPropertyId              // 30083
    // UIA_TableItemRowHeaderItemsPropertyId,           // 30084
    // UIA_TableItemColumnHeaderItemsPropertyId,        // 30085
    // UIA_ToggleToggleStatePropertyId                  // 30086
    // UIA_TransformCanMovePropertyId,                  // 30087
    // UIA_TransformCanResizePropertyId,                // 30088
    // UIA_TransformCanRotatePropertyId,                // 30089
    UIA_IsLegacyIAccessiblePatternAvailablePropertyId,  // 30090
    // UIA_LegacyIAccessible*                           // 30091-30100
    UIA_AriaRolePropertyId,                             // 30101
    UIA_AriaPropertiesPropertyId,                       // 30102
    UIA_IsDataValidForFormPropertyId,                   // 30103
    UIA_ControllerForPropertyId,                        // 30104
    UIA_DescribedByPropertyId,                          // 30105
    UIA_FlowsToPropertyId,                              // 30106
    // UIA_ProviderDescriptionPropertyId                // 30107
    UIA_IsItemContainerPatternAvailablePropertyId,      // 30108
    UIA_IsVirtualizedItemPatternAvailablePropertyId,    // 30109
    UIA_IsSynchronizedInputPatternAvailablePropertyId,  // 30110
    UIA_OptimizeForVisualContentPropertyId,             // 30111
    UIA_IsObjectModelPatternAvailablePropertyId,        // 30112
    UIA_AnnotationAnnotationTypeIdPropertyId,           // 30113
    UIA_AnnotationAnnotationTypeNamePropertyId,         // 30114
    UIA_AnnotationAuthorPropertyId,                     // 30115
    UIA_AnnotationDateTimePropertyId,                   // 30116
    UIA_AnnotationTargetPropertyId,                     // 30117
    UIA_IsAnnotationPatternAvailablePropertyId,         // 30118
    UIA_IsTextPattern2AvailablePropertyId,              // 30119
    UIA_StylesStyleIdPropertyId,                        // 30120
    UIA_StylesStyleNamePropertyId,                      // 30121
    UIA_StylesFillColorPropertyId,                      // 30122
    UIA_StylesFillPatternStylePropertyId,               // 30123
    UIA_StylesShapePropertyId,                          // 30124
    UIA_StylesFillPatternColorPropertyId,               // 30125
    UIA_StylesExtendedPropertiesPropertyId,             // 30126
    UIA_IsStylesPatternAvailablePropertyId,             // 30127
    UIA_IsSpreadsheetPatternAvailablePropertyId,        // 30128
    UIA_SpreadsheetItemFormulaPropertyId,               // 30129
    UIA_SpreadsheetItemAnnotationObjectsPropertyId,     // 30130
    UIA_SpreadsheetItemAnnotationTypesPropertyId,       // 30131
    UIA_IsSpreadsheetItemPatternAvailablePropertyId,    // 30132
    UIA_Transform2CanZoomPropertyId,                    // 30133
    UIA_IsTransformPattern2AvailablePropertyId,         // 30134
    UIA_LiveSettingPropertyId,                          // 30135
    UIA_IsTextChildPatternAvailablePropertyId,          // 30136
    UIA_IsDragPatternAvailablePropertyId,               // 30137
    UIA_DragIsGrabbedPropertyId,                        // 30138
    UIA_DragDropEffectPropertyId,                       // 30139
    UIA_DragDropEffectsPropertyId,                      // 30140
    UIA_IsDropTargetPatternAvailablePropertyId,         // 30141
    UIA_DropTargetDropTargetEffectPropertyId,           // 30142
    UIA_DropTargetDropTargetEffectsPropertyId,          // 30143
    UIA_DragGrabbedItemsPropertyId,                     // 30144
    UIA_Transform2ZoomLevelPropertyId,                  // 30145
    UIA_Transform2ZoomMinimumPropertyId,                // 30146
    UIA_Transform2ZoomMaximumPropertyId,                // 30147
    UIA_FlowsFromPropertyId,                            // 30148
    UIA_IsTextEditPatternAvailablePropertyId,           // 30149
    UIA_IsPeripheralPropertyId,                         // 30150
    UIA_IsCustomNavigationPatternAvailablePropertyId,   // 30151
    UIA_PositionInSetPropertyId,                        // 30152
    UIA_SizeOfSetPropertyId,                            // 30153
    UIA_LevelPropertyId,                                // 30154
    UIA_AnnotationTypesPropertyId,                      // 30155
    UIA_AnnotationObjectsPropertyId,                    // 30156
    UIA_LandmarkTypePropertyId,                         // 30157
    UIA_LocalizedLandmarkTypePropertyId,                // 30158
    UIA_FullDescriptionPropertyId,                      // 30159
    UIA_FillColorPropertyId,                            // 30160
    UIA_OutlineColorPropertyId,                         // 30161
    UIA_FillTypePropertyId,                             // 30162
    UIA_VisualEffectsPropertyId,                        // 30163
    UIA_OutlineThicknessPropertyId,                     // 30164
    UIA_CenterPointPropertyId,                          // 30165
    UIA_RotationPropertyId,                             // 30166
    UIA_SizePropertyId,                                 // 30167
    UIA_IsSelectionPattern2AvailablePropertyId,         // 30168
    UIA_Selection2FirstSelectedItemPropertyId,          // 30169
    UIA_Selection2LastSelectedItemPropertyId,           // 30170
    UIA_Selection2CurrentSelectedItemPropertyId,        // 30171
    UIA_Selection2ItemCountPropertyId,                  // 30172
    UIA_HeadingLevelPropertyId,                         // 30173
};

const long AccessibilityTreeFormatterUia::patterns_[] = {
    UIA_SelectionPatternId,       // 10001
    UIA_ValuePatternId,           // 10002
    UIA_RangeValuePatternId,      // 10003
    UIA_ScrollPatternId,          // 10004
    UIA_ExpandCollapsePatternId,  // 10005
    UIA_GridPatternId,            // 10006
    UIA_GridItemPatternId,        // 10007
    UIA_WindowPatternId,          // 10009
    UIA_SelectionItemPatternId,   // 10010
    UIA_TablePatternId,           // 10012
    UIA_TogglePatternId,          // 10015
};

const long AccessibilityTreeFormatterUia::pattern_properties_[] = {
    UIA_ValueValuePropertyId,                         // 30045
    UIA_ValueIsReadOnlyPropertyId,                    // 30046
    UIA_RangeValueValuePropertyId,                    // 30047
    UIA_RangeValueIsReadOnlyPropertyId,               // 30048
    UIA_RangeValueMinimumPropertyId,                  // 30049
    UIA_RangeValueMaximumPropertyId,                  // 30050
    UIA_RangeValueLargeChangePropertyId,              // 30051
    UIA_RangeValueSmallChangePropertyId,              // 30052
    UIA_ScrollHorizontalScrollPercentPropertyId,      // 30053
    UIA_ScrollHorizontalViewSizePropertyId,           // 30054
    UIA_ScrollVerticalScrollPercentPropertyId,        // 30055
    UIA_ScrollVerticalViewSizePropertyId,             // 30056
    UIA_ScrollHorizontallyScrollablePropertyId,       // 30057
    UIA_ScrollVerticallyScrollablePropertyId,         // 30058
    UIA_SelectionCanSelectMultiplePropertyId,         // 30060
    UIA_SelectionIsSelectionRequiredPropertyId,       // 30061
    UIA_GridRowCountPropertyId,                       // 30062
    UIA_GridColumnCountPropertyId,                    // 30063
    UIA_GridItemRowPropertyId,                        // 30064
    UIA_GridItemColumnPropertyId,                     // 30065
    UIA_GridItemRowSpanPropertyId,                    // 30066
    UIA_GridItemColumnSpanPropertyId,                 // 30067
    UIA_GridItemContainingGridPropertyId,             // 30068
    UIA_ExpandCollapseExpandCollapseStatePropertyId,  // 30070
    UIA_WindowIsModalPropertyId,                      // 30077
    UIA_SelectionItemIsSelectedPropertyId,            // 30079
    UIA_SelectionItemSelectionContainerPropertyId,    // 30080
    UIA_TableRowOrColumnMajorPropertyId,              // 30083
    UIA_ToggleToggleStatePropertyId,                  // 30086
};
// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatterUia::CreateUia() {
  base::win::AssertComInitialized();
  return std::make_unique<AccessibilityTreeFormatterUia>();
}

AccessibilityTreeFormatterUia::AccessibilityTreeFormatterUia() {
  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());
  BuildCacheRequests();
}

AccessibilityTreeFormatterUia::~AccessibilityTreeFormatterUia() {}

void AccessibilityTreeFormatterUia::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  // Too noisy: IsKeyboardFocusable, IsDataValidForForm, UIA_ScrollPatternId,
  //  Value.IsReadOnly

  // properties not exposed through a pattern
  AddPropertyFilter(property_filters, "Name=*");
  AddPropertyFilter(property_filters, "ItemStatus=*");
  AddPropertyFilter(property_filters, "Orientation=OrientationType_Horizontal");
  AddPropertyFilter(property_filters, "IsPassword=true");
  AddPropertyFilter(property_filters, "IsControlElement=false");
  AddPropertyFilter(property_filters, "IsEnabled=false");
  AddPropertyFilter(property_filters, "IsRequiredForForm=true");

  // UIA_ExpandCollapsePatternId
  AddPropertyFilter(property_filters, "ExpandCollapse.ExpandCollapseState=*");

  // UIA_GridPatternId
  AddPropertyFilter(property_filters, "Grid.ColumnCount=*");
  AddPropertyFilter(property_filters, "Grid.RowCount=*");
  // UIA_GridItemPatternId
  AddPropertyFilter(property_filters, "GridItem.Column=*");
  AddPropertyFilter(property_filters, "GridItem.ColumnSpan=*");
  AddPropertyFilter(property_filters, "GridItem.Row=*");
  AddPropertyFilter(property_filters, "GridItem.RowSpan=*");
  AddPropertyFilter(property_filters, "GridItem.ContainingGrid=*");
  // UIA_RangeValuePatternId
  AddPropertyFilter(property_filters, "RangeValue.IsReadOnly=*");
  AddPropertyFilter(property_filters, "RangeValue.LargeChange=*");
  AddPropertyFilter(property_filters, "RangeValue.SmallChange=*");
  AddPropertyFilter(property_filters, "RangeValue.Maximum=*");
  AddPropertyFilter(property_filters, "RangeValue.Minimum=*");
  AddPropertyFilter(property_filters, "RangeValue.Value=*");
  // UIA_SelectionPatternId
  AddPropertyFilter(property_filters, "Selection.CanSelectMultiple=*");
  AddPropertyFilter(property_filters, "Selection.IsSelectionRequired=*");
  // UIA_SelectionItemPatternId
  AddPropertyFilter(property_filters, "SelectionItem.IsSelected=*");
  AddPropertyFilter(property_filters, "SelectionItem.SelectionContainer=*");
  // UIA_TablePatternId
  AddPropertyFilter(property_filters, "Table.RowOrColumnMajor=*");
  // UIA_TogglePatternId
  AddPropertyFilter(property_filters, "Toggle.ToggleState=*");
  // UIA_ValuePatternId
  AddPropertyFilter(property_filters, "Value.Value=*");
  AddPropertyFilter(property_filters, "Value.Value='http*'",
                    PropertyFilter::DENY);
  // UIA_WindowPatternId
  AddPropertyFilter(property_filters, "Window.IsModal=*");
}

// static
void AccessibilityTreeFormatterUia::SetUpCommandLineForTestPass(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(::switches::kEnableExperimentalUIAutomation);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTree(
    BrowserAccessibility* start) {
  // We use the UI Automation client API to produce the tree dump, but
  // BrowserAccessibility has a pointer to a provider API implementation, and
  // we can't directly relate the two -- the OS manages the relationship.
  // To locate the client element we want, we'll construct a RuntimeId
  // corresponding to our provider element, then search for that.

  // Start by getting the root element for the HWND hosting the web content.
  HWND hwnd =
      start->manager()->GetRoot()->GetTargetForNativeAccessibilityEvent();
  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia_->ElementFromHandle(hwnd, &root);
  CHECK(root.Get());

  // Get the bounds of the root element, to pass into tree building later.
  RECT root_bounds = {0};
  root->get_CurrentBoundingRectangle(&root_bounds);

  // The root element is provided by AXFragmentRootWin, whose RuntimeId is not
  // in the same form as elements provided by BrowserAccessibility.
  // Find the root element's first child, which should be provided by
  // BrowserAccessibility. We'll use that element's RuntimeId as a template for
  // the RuntimeId of the element we're looking for.
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  uia_->get_RawViewWalker(&tree_walker);
  Microsoft::WRL::ComPtr<IUIAutomationElement> first_child;
  tree_walker->GetFirstChildElement(root.Get(), &first_child);
  CHECK(first_child.Get());

  // Get first_child's RuntimeId and swap out the last element in its SAFEARRAY
  // for the UniqueId of the element we want to start from.
  base::win::ScopedSafearray runtime_id;
  first_child->GetRuntimeId(runtime_id.Receive());
  CHECK(runtime_id.Get());
  LONG lower_bound = 0;
  HRESULT hr = ::SafeArrayGetLBound(runtime_id.Get(), 1, &lower_bound);
  CHECK(SUCCEEDED(hr));
  LONG upper_bound = 0;
  hr = ::SafeArrayGetUBound(runtime_id.Get(), 1, &upper_bound);
  CHECK(SUCCEEDED(hr));
  {
    int32_t* runtime_id_array = nullptr;
    ::SafeArrayAccessData(runtime_id.Get(),
                          reinterpret_cast<void**>(&runtime_id_array));
    CHECK(runtime_id_array);
    CHECK((upper_bound - lower_bound) >= 0);
    runtime_id_array[upper_bound - lower_bound] = start->GetUniqueId().Get();
    ::SafeArrayUnaccessData(runtime_id.Get());
  }

  // Find the element with the desired RuntimeId.
  base::win::ScopedVariant runtime_id_variant(runtime_id.Release());
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  uia_->CreatePropertyCondition(UIA_RuntimeIdPropertyId, runtime_id_variant,
                                &condition);
  CHECK(condition);
  Microsoft::WRL::ComPtr<IUIAutomationElement> start_element;

  root->FindFirst(TreeScope_Subtree, condition.Get(), &start_element);
  std::unique_ptr<base::DictionaryValue> tree =
      std::make_unique<base::DictionaryValue>();

  if (start_element.Get()) {
    // Build an accessibility tree starting from that element.
    RecursiveBuildAccessibilityTree(start_element.Get(), root_bounds.left,
                                    root_bounds.top, tree.get());
  } else {
    // If the search failed, start dumping with the first thing that isn't a
    // Pane.
    // TODO(http://crbug.com/1071188): Figure out why the original FindFirst
    // fails and remove this fallback codepath.
    Microsoft::WRL::ComPtr<IUIAutomationElement> non_pane_descendant;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> is_pane_condition;
    base::win::ScopedVariant pane_control_type_variant(UIA_PaneControlTypeId);
    uia_->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                  pane_control_type_variant,
                                  &is_pane_condition);
    Microsoft::WRL::ComPtr<IUIAutomationCondition> not_is_pane_condition;
    uia_->CreateNotCondition(is_pane_condition.Get(), &not_is_pane_condition);
    root->FindFirst(TreeScope_Subtree, not_is_pane_condition.Get(),
                    &non_pane_descendant);

    DCHECK(non_pane_descendant.Get());
    RecursiveBuildAccessibilityTree(non_pane_descendant.Get(), root_bounds.left,
                                    root_bounds.top, tree.get());
  }
  return tree;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTreeForWindow(
    gfx::AcceleratedWidget hwnd) {
  CHECK(hwnd);

  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia_->ElementFromHandle(hwnd, &root);
  CHECK(root.Get());

  RECT root_bounds = {0};
  root->get_CurrentBoundingRectangle(&root_bounds);

  std::unique_ptr<base::DictionaryValue> tree =
      std::make_unique<base::DictionaryValue>();
  RecursiveBuildAccessibilityTree(root.Get(), root_bounds.left, root_bounds.top,
                                  tree.get());
  return tree;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTreeForSelector(
    const TreeSelector& selector) {
  LOG(ERROR) << "Windows does not yet support building accessibility trees for "
                "tree selectors";
  return nullptr;
}

void AccessibilityTreeFormatterUia::RecursiveBuildAccessibilityTree(
    IUIAutomationElement* uncached_node,
    int root_x,
    int root_y,
    base::DictionaryValue* dict) {
  // Process this node.
  AddProperties(uncached_node, root_x, root_y, dict);

  // Update the cache to get children
  Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
  uncached_node->BuildUpdatedCache(children_cache_request_.Get(), &parent);

  Microsoft::WRL::ComPtr<IUIAutomationElementArray> children;
  if (!SUCCEEDED(parent->GetCachedChildren(&children)) || !children)
    return;
  // Process children.
  auto child_list = std::make_unique<base::ListValue>();
  int child_count;
  children->get_Length(&child_count);
  for (int i = 0; i < child_count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> child;
    std::unique_ptr<base::DictionaryValue> child_dict =
        std::make_unique<base::DictionaryValue>();
    if (SUCCEEDED(children->GetElement(i, &child))) {
      RecursiveBuildAccessibilityTree(child.Get(), root_x, root_y,
                                      child_dict.get());
    } else {
      child_dict->SetString("error", L"[Error retrieving child]");
    }
    child_list->Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(child_list));
}

void AccessibilityTreeFormatterUia::AddProperties(
    IUIAutomationElement* uncached_node,
    int root_x,
    int root_y,
    base::DictionaryValue* dict) {
  // Update the cache for this node's information.
  Microsoft::WRL::ComPtr<IUIAutomationElement> node;
  uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

  // Get all properties that may be on this node.
  for (long i : properties_) {
    base::win::ScopedVariant variant;
    if (SUCCEEDED(node->GetCachedPropertyValue(i, variant.Receive()))) {
      WriteProperty(i, variant, root_x, root_y, dict);
    }
  }
  // Add control pattern specific properties
  AddExpandCollapseProperties(node.Get(), dict);
  AddGridProperties(node.Get(), dict);
  AddGridItemProperties(node.Get(), dict);
  AddRangeValueProperties(node.Get(), dict);
  AddScrollProperties(node.Get(), dict);
  AddSelectionProperties(node.Get(), dict);
  AddSelectionItemProperties(node.Get(), dict);
  AddTableProperties(node.Get(), dict);
  AddToggleProperties(node.Get(), dict);
  AddValueProperties(node.Get(), dict);
  AddValueProperties(node.Get(), dict);
  AddWindowProperties(node.Get(), dict);
}

void AccessibilityTreeFormatterUia::AddExpandCollapseProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationExpandCollapsePattern>
      expand_collapse_pattern;
  if (SUCCEEDED(
          node->GetCachedPatternAs(UIA_ExpandCollapsePatternId,
                                   IID_PPV_ARGS(&expand_collapse_pattern))) &&
      expand_collapse_pattern) {
    ExpandCollapseState current_state;
    if (SUCCEEDED(expand_collapse_pattern->get_CachedExpandCollapseState(
            &current_state))) {
      base::string16 state;
      switch (current_state) {
        case ExpandCollapseState_Collapsed:
          state = L"Collapsed";
          break;
        case ExpandCollapseState_Expanded:
          state = L"Expanded";
          break;
        case ExpandCollapseState_PartiallyExpanded:
          state = L"PartiallyExpanded";
          break;
        case ExpandCollapseState_LeafNode:
          state = L"LeafNode";
          break;
      }
      dict->SetString("ExpandCollapse.ExpandCollapseState", state);
    }
  }
}

void AccessibilityTreeFormatterUia::AddGridProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationGridPattern> grid_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_GridPatternId,
                                         IID_PPV_ARGS(&grid_pattern))) &&
      grid_pattern) {
    int column_count;
    if (SUCCEEDED(grid_pattern->get_CachedColumnCount(&column_count)))
      dict->SetInteger("Grid.ColumnCount", column_count);

    int row_count;
    if (SUCCEEDED(grid_pattern->get_CachedRowCount(&row_count)))
      dict->SetInteger("Grid.RowCount", row_count);
  }
}

void AccessibilityTreeFormatterUia::AddGridItemProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationGridItemPattern> grid_item_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_GridItemPatternId,
                                         IID_PPV_ARGS(&grid_item_pattern))) &&
      grid_item_pattern) {
    int column;
    if (SUCCEEDED(grid_item_pattern->get_CachedColumn(&column)))
      dict->SetInteger("GridItem.Column", column);

    int column_span;
    if (SUCCEEDED(grid_item_pattern->get_CachedColumnSpan(&column_span)))
      dict->SetInteger("GridItem.ColumnSpan", column_span);

    int row;
    if (SUCCEEDED(grid_item_pattern->get_CachedRow(&row)))
      dict->SetInteger("GridItem.Row", row);

    int row_span;
    if (SUCCEEDED(grid_item_pattern->get_CachedRowSpan(&row_span)))
      dict->SetInteger("GridItem.RowSpan", row_span);
    Microsoft::WRL::ComPtr<IUIAutomationElement> containing_grid;
    if (SUCCEEDED(
            grid_item_pattern->get_CachedContainingGrid(&containing_grid))) {
      dict->SetString("GridItem.ContainingGrid",
                      GetNodeName(containing_grid.Get()));
      ;
    }
  }
}

void AccessibilityTreeFormatterUia::AddRangeValueProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationRangeValuePattern> range_value_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_RangeValuePatternId,
                                         IID_PPV_ARGS(&range_value_pattern))) &&
      range_value_pattern) {
    BOOL is_read_only;
    if (SUCCEEDED(range_value_pattern->get_CachedIsReadOnly(&is_read_only)))
      dict->SetBoolean("RangeValue.IsReadOnly", is_read_only);

    double large_change;
    if (SUCCEEDED(range_value_pattern->get_CachedLargeChange(&large_change)))
      dict->SetDouble("RangeValue.LargeChange", large_change);

    double small_change;
    if (SUCCEEDED(range_value_pattern->get_CachedSmallChange(&small_change)))
      dict->SetDouble("RangeValue.SmallChange", small_change);

    double maximum;
    if (SUCCEEDED(range_value_pattern->get_CachedMaximum(&maximum)))
      dict->SetDouble("RangeValue.Maximum", maximum);

    double minimum;
    if (SUCCEEDED(range_value_pattern->get_CachedMinimum(&minimum)))
      dict->SetDouble("RangeValue.Minimum", minimum);

    double value;
    if (SUCCEEDED(range_value_pattern->get_CachedValue(&value)))
      dict->SetDouble("RangeValue.Value", value);
  }
}

void AccessibilityTreeFormatterUia::AddScrollProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationScrollPattern> scroll_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_ScrollPatternId,
                                         IID_PPV_ARGS(&scroll_pattern))) &&
      scroll_pattern) {
    double horizontal_scroll_percent;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontalScrollPercent(
            &horizontal_scroll_percent))) {
      dict->SetDouble("Scroll.HorizontalScrollPercent",
                      horizontal_scroll_percent);
    }

    double horizontal_view_size;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontalViewSize(
            &horizontal_view_size)))
      dict->SetDouble("Scroll.HorizontalViewSize", horizontal_view_size);

    BOOL horizontally_scrollable;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontallyScrollable(
            &horizontally_scrollable))) {
      dict->SetBoolean("Scroll.HorizontallyScrollable",
                       horizontally_scrollable);
    }

    double vertical_scroll_percent;
    if (SUCCEEDED(scroll_pattern->get_CachedVerticalScrollPercent(
            &vertical_scroll_percent)))
      dict->SetDouble("Scroll.VerticalScrollPercent", vertical_scroll_percent);

    double vertical_view_size;
    if (SUCCEEDED(
            scroll_pattern->get_CachedVerticalViewSize(&vertical_view_size)))
      dict->SetDouble("Scroll.VerticalViewSize", vertical_view_size);

    BOOL vertically_scrollable;
    if (SUCCEEDED(scroll_pattern->get_CachedVerticallyScrollable(
            &vertically_scrollable)))
      dict->SetBoolean("Scroll.VerticallyScrollable", vertically_scrollable);
  }
}

void AccessibilityTreeFormatterUia::AddSelectionProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationSelectionPattern> selection_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_SelectionPatternId,
                                         IID_PPV_ARGS(&selection_pattern))) &&
      selection_pattern) {
    BOOL can_select_multiple;
    if (SUCCEEDED(selection_pattern->get_CachedCanSelectMultiple(
            &can_select_multiple)))
      dict->SetBoolean("Selection.CanSelectMultiple", can_select_multiple);

    BOOL is_selection_required;
    if (SUCCEEDED(selection_pattern->get_CachedIsSelectionRequired(
            &is_selection_required)))
      dict->SetBoolean("Selection.IsSelectionRequired", is_selection_required);
  }
}

void AccessibilityTreeFormatterUia::AddSelectionItemProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern>
      selection_item_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(
          UIA_SelectionItemPatternId, IID_PPV_ARGS(&selection_item_pattern))) &&
      selection_item_pattern) {
    BOOL is_selected;
    if (SUCCEEDED(selection_item_pattern->get_CachedIsSelected(&is_selected)))
      dict->SetBoolean("SelectionItem.IsSelected", is_selected);

    Microsoft::WRL::ComPtr<IUIAutomationElement> selection_container;
    if (SUCCEEDED(selection_item_pattern->get_CachedSelectionContainer(
            &selection_container))) {
      dict->SetString("SelectionItem.SelectionContainer",
                      GetNodeName(selection_container.Get()));
    }
  }
}

void AccessibilityTreeFormatterUia::AddTableProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationTablePattern> table_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_TablePatternId,
                                         IID_PPV_ARGS(&table_pattern))) &&
      table_pattern) {
    RowOrColumnMajor row_or_column_major;
    if (SUCCEEDED(
            table_pattern->get_CachedRowOrColumnMajor(&row_or_column_major))) {
      base::string16 row_or_column_string;
      switch (row_or_column_major) {
        case RowOrColumnMajor_RowMajor:
          row_or_column_string = L"RowMajor";
          break;
        case RowOrColumnMajor_ColumnMajor:
          row_or_column_string = L"ColumnMajor";
          break;
        case RowOrColumnMajor_Indeterminate:
          row_or_column_string = L"Indeterminate";
          break;
      }
      dict->SetString("Table.RowOrColumnMajor", row_or_column_string);
    }
  }
}

void AccessibilityTreeFormatterUia::AddToggleProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationTogglePattern> toggle_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_TogglePatternId,
                                         IID_PPV_ARGS(&toggle_pattern))) &&
      toggle_pattern) {
    ToggleState toggle_state;
    if (SUCCEEDED(toggle_pattern->get_CachedToggleState(&toggle_state))) {
      base::string16 toggle_state_string;
      switch (toggle_state) {
        case ToggleState_Off:
          toggle_state_string = L"Off";
          break;
        case ToggleState_On:
          toggle_state_string = L"On";
          break;
        case ToggleState_Indeterminate:
          toggle_state_string = L"Indeterminate";
          break;
      }
      dict->SetString("Toggle.ToggleState", toggle_state_string);
    }
  }
}

void AccessibilityTreeFormatterUia::AddValueProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationValuePattern> value_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_ValuePatternId,
                                         IID_PPV_ARGS(&value_pattern))) &&
      value_pattern) {
    BOOL is_read_only;
    if (SUCCEEDED(value_pattern->get_CachedIsReadOnly(&is_read_only)))
      dict->SetBoolean("Value.IsReadOnly", is_read_only);

    base::win::ScopedBstr value;
    if (SUCCEEDED(value_pattern->get_CachedValue(value.Receive())))
      dict->SetString("Value.Value", BstrToUTF8(value.Get()));
  }
}

void AccessibilityTreeFormatterUia::AddWindowProperties(
    IUIAutomationElement* node,
    base::DictionaryValue* dict) {
  Microsoft::WRL::ComPtr<IUIAutomationWindowPattern> window_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_WindowPatternId,
                                         IID_PPV_ARGS(&window_pattern))) &&
      window_pattern) {
    BOOL is_modal;
    if (SUCCEEDED(window_pattern->get_CachedIsModal(&is_modal)))
      dict->SetBoolean("Window.IsModal", is_modal);
  }
}

void AccessibilityTreeFormatterUia::WriteProperty(
    long propertyId,
    const base::win::ScopedVariant& var,
    int root_x,
    int root_y,
    base::DictionaryValue* dict) {
  switch (var.type()) {
    case VT_EMPTY:
    case VT_NULL:
      break;
    case VT_I2:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->iVal);
      break;
    case VT_I4:
      WriteI4Property(propertyId, var.ptr()->lVal, dict);
      break;
    case VT_R4:
      dict->SetDouble(UiaIdentifierToCondensedString(propertyId),
                      var.ptr()->fltVal);
      break;
    case VT_R8:
      dict->SetDouble(UiaIdentifierToCondensedString(propertyId),
                      var.ptr()->dblVal);
      break;
    case VT_I1:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->cVal);
      break;
    case VT_UI1:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->bVal);
      break;
    case VT_UI2:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->uiVal);
      break;
    case VT_UI4:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->ulVal);
      break;
      break;
    case VT_BSTR:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      BstrToUTF8(var.ptr()->bstrVal));
      break;
    case VT_BOOL:
      dict->SetBoolean(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->boolVal == VARIANT_TRUE ? true : false);
      break;
    case VT_UNKNOWN:
      WriteUnknownProperty(propertyId, var.ptr()->punkVal, dict);
      break;
    default:
      switch (propertyId) {
        case UIA_BoundingRectanglePropertyId:
          WriteRectangleProperty(propertyId, var, root_x, root_y, dict);
          break;
        default:
          break;
      }
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteI4Property(
    long propertyId,
    long lval,
    base::DictionaryValue* dict) {
  switch (propertyId) {
    case UIA_ControlTypePropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaIdentifierToCondensedString(lval));
      break;
    case UIA_OrientationPropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaOrientationToString(lval));
      break;
    case UIA_LiveSettingPropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaLiveSettingToString(lval));
      break;
    default:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId), lval);
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteUnknownProperty(
    long propertyId,
    IUnknown* unk,
    base::DictionaryValue* dict) {
  switch (propertyId) {
    case UIA_ControllerForPropertyId:
    case UIA_DescribedByPropertyId:
    case UIA_FlowsFromPropertyId:
    case UIA_FlowsToPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElementArray> array;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&array))))
        WriteElementArray(propertyId, array.Get(), dict);
      break;
    }
    case UIA_LabeledByPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElement> node;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&node)))) {
        dict->SetString(UiaIdentifierToCondensedString(propertyId),
                        GetNodeName(node.Get()));
      }
      break;
    }
    default:
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteRectangleProperty(
    long propertyId,
    const VARIANT& value,
    int root_x,
    int root_y,
    base::DictionaryValue* dict) {
  CHECK(value.vt == (VT_ARRAY | VT_R8));

  double* data = nullptr;
  SafeArrayAccessData(value.parray, reinterpret_cast<void**>(&data));

  auto rectangle = std::make_unique<base::DictionaryValue>();
  rectangle->SetInteger("left", data[0] - root_x);
  rectangle->SetInteger("top", data[1] - root_y);
  rectangle->SetInteger("width", data[2]);
  rectangle->SetInteger("height", data[3]);
  dict->Set(UiaIdentifierToCondensedString(propertyId), std::move(rectangle));

  SafeArrayUnaccessData(value.parray);
}

void AccessibilityTreeFormatterUia::WriteElementArray(
    long propertyId,
    IUIAutomationElementArray* array,
    base::DictionaryValue* dict) {
  int count;
  array->get_Length(&count);
  base::string16 element_list;
  for (int i = 0; i < count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> element;
    if (SUCCEEDED(array->GetElement(i, &element))) {
      if (element_list != L"") {
        element_list += L", ";
      }
      auto name = GetNodeName(element.Get());
      if (name.empty()) {
        base::win::ScopedBstr role;
        element->get_CurrentAriaRole(role.Receive());
        name = L"{" + base::string16(role.Get()) + L"}";
      }
      element_list += name;
    }
  }
  if (!element_list.empty())
    dict->SetString(UiaIdentifierToCondensedString(propertyId), element_list);
}

base::string16 AccessibilityTreeFormatterUia::GetNodeName(
    IUIAutomationElement* uncached_node) {
  // Update the cache for this node.
  if (uncached_node) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> node;
    uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

    base::win::ScopedBstr name;
    base::win::ScopedVariant variant;
    if (SUCCEEDED(node->GetCachedPropertyValue(UIA_NamePropertyId,
                                               variant.Receive())) &&
        variant.type() == VT_BSTR) {
      return base::string16(variant.ptr()->bstrVal,
                            SysStringLen(variant.ptr()->bstrVal));
    }
  }
  return L"";
}

void AccessibilityTreeFormatterUia::BuildCacheRequests() {
  // Create cache request for requesting children of a node.
  uia_->CreateCacheRequest(&children_cache_request_);
  CHECK(children_cache_request_.Get());
  children_cache_request_->put_TreeScope(TreeScope_Children);

  // Set filter to include all nodes in the raw view.
  Microsoft::WRL::ComPtr<IUIAutomationCondition> raw_view_condition;
  uia_->get_RawViewCondition(&raw_view_condition);
  CHECK(raw_view_condition.Get());
  children_cache_request_->put_TreeFilter(raw_view_condition.Get());

  // Create cache request for requesting information about a node.
  uia_->CreateCacheRequest(&element_cache_request_);
  CHECK(element_cache_request_.Get());
  element_cache_request_->put_TreeScope(TreeScope_Element);

  // Caching properties allows us to use GetCachedPropertyValue.
  // The non-cached version (GetCurrentPropertyValue) may cross
  // the process boundary for each call.

  // Cache all properties.
  for (long i : properties_) {
    element_cache_request_->AddProperty(i);
  }
  // Cache all patterns.
  for (long i : patterns_) {
    element_cache_request_->AddPattern(i);
  }
  // Cache pattern properties
  for (long i : pattern_properties_) {
    element_cache_request_->AddProperty(i);
  }
}

std::string AccessibilityTreeFormatterUia::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_result) {
  std::unique_ptr<base::DictionaryValue> tree;
  std::string line;

  // Always show control type, and show it first.
  std::string control_type_value;
  dict.GetString(UiaIdentifierToCondensedString(UIA_ControlTypePropertyId),
                 &control_type_value);
  WriteAttribute(true, control_type_value, &line);
  if (filtered_result) {
    filtered_result->SetString(
        UiaIdentifierToStringUTF8(UIA_ControlTypePropertyId),
        control_type_value);
  }

  // properties
  for (long i : properties_) {
    ProcessPropertyForOutput(UiaIdentifierToCondensedString(i), dict, line,
                             filtered_result);
  }

  // patterns
  const std::string pattern_property_names[] = {
      // UIA_ExpandCollapsePatternId
      "ExpandCollapse.ExpandCollapseState",
      // UIA_GridPatternId
      "Grid.ColumnCount", "Grid.RowCount",
      // UIA_GridItemPatternId
      "GridItem.Column", "GridItem.ColumnSpan", "GridItem.Row",
      "GridItem.RowSpan", "GridItem.ContainingGrid",
      // UIA_RangeValuePatternId
      "RangeValue.IsReadOnly", "RangeValue.LargeChange",
      "RangeValue.SmallChange", "RangeValue.Maximum", "RangeValue.Minimum",
      "RangeValue.Value",
      // UIA_ScrollPatternId
      "Scroll.HorizontalScrollPercent", "Scroll.HorizontalViewSize",
      "Scroll.HorizontallyScrollable", "Scroll.VerticalScrollPercent",
      "Scroll.VerticalViewSize", "Scroll.VerticallyScrollable",
      // UIA_SelectionPatternId
      "Selection.CanSelectMultiple", "Selection.IsSelectionRequired",
      // UIA_SelectionItemPatternId
      "SelectionItem.IsSelected", "SelectionItem.SelectionContainer",
      // UIA_TablePatternId
      "Table.RowOrColumnMajor",
      // UIA_TogglePatternId
      "Toggle.ToggleState",
      // UIA_ValuePatternId
      "Value.IsReadOnly", "Value.Value",
      // UIA_WindowPatternId
      "Window.IsModal"};

  for (const std::string& pattern_property_name : pattern_property_names) {
    ProcessPropertyForOutput(pattern_property_name, dict, line,
                             filtered_result);
  }

  return line;
}

void AccessibilityTreeFormatterUia::ProcessPropertyForOutput(
    const std::string& property_name,
    const base::DictionaryValue& dict,
    std::string& line,
    base::DictionaryValue* filtered_result) {
  //
  const base::Value* value;
  if (dict.Get(property_name, &value))
    ProcessValueForOutput(property_name, value, line, filtered_result);
}

void AccessibilityTreeFormatterUia::ProcessValueForOutput(
    const std::string& name,
    const base::Value* value,
    std::string& line,
    base::DictionaryValue* filtered_result) {
  switch (value->type()) {
    case base::Value::Type::STRING: {
      std::string string_value;
      value->GetAsString(&string_value);
      bool did_pass_filters = WriteAttribute(
          false,
          base::StringPrintf("%s='%s'", name.c_str(), string_value.c_str()),
          &line);
      if (filtered_result && did_pass_filters)
        filtered_result->SetString(name, string_value);
      break;
    }
    case base::Value::Type::BOOLEAN: {
      bool bool_value = 0;
      value->GetAsBoolean(&bool_value);
      bool did_pass_filters =
          WriteAttribute(false,
                         base::StringPrintf("%s=%s", name.c_str(),
                                            (bool_value ? "true" : "false")),
                         &line);
      if (filtered_result && did_pass_filters)
        filtered_result->SetBoolean(name, bool_value);
      break;
    }
    case base::Value::Type::INTEGER: {
      int int_value = 0;
      value->GetAsInteger(&int_value);
      bool did_pass_filters = WriteAttribute(
          false, base::StringPrintf("%s=%d", name.c_str(), int_value), &line);
      if (filtered_result && did_pass_filters)
        filtered_result->SetInteger(name, int_value);
      break;
    }
    case base::Value::Type::DOUBLE: {
      double double_value = 0.0;
      value->GetAsDouble(&double_value);
      bool did_pass_filters = WriteAttribute(
          false, base::StringPrintf("%s=%.2f", name.c_str(), double_value),
          &line);
      if (filtered_result && did_pass_filters)
        filtered_result->SetDouble(name, double_value);
      break;
    }
    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* dict_value = nullptr;
      value->GetAsDictionary(&dict_value);
      bool did_pass_filters = false;
      if (name == "BoundingRectangle") {
        did_pass_filters =
            WriteAttribute(false,
                           FormatRectangle(*dict_value, "BoundingRectangle",
                                           "left", "top", "width", "height"),
                           &line);
      }
      if (filtered_result && did_pass_filters)
        filtered_result->SetKey(name, dict_value->Clone());
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

base::FilePath::StringType
AccessibilityTreeFormatterUia::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-uia-win.txt");
}

base::FilePath::StringType
AccessibilityTreeFormatterUia::GetVersionSpecificExpectedFileSuffix() {
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    return FILE_PATH_LITERAL("-expected-uia-win7.txt");
  }
  return FILE_PATH_LITERAL("");
}

const std::string AccessibilityTreeFormatterUia::GetAllowEmptyString() {
  return "@UIA-WIN-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterUia::GetAllowString() {
  return "@UIA-WIN-ALLOW:";
}

const std::string AccessibilityTreeFormatterUia::GetDenyString() {
  return "@UIA-WIN-DENY:";
}

const std::string AccessibilityTreeFormatterUia::GetDenyNodeString() {
  return "@UIA-WIN-DENY-NODE:";
}

const std::string AccessibilityTreeFormatterUia::GetRunUntilEventString() {
  return "@UIA-WIN-RUN-UNTIL-EVENT:";
}

}  // namespace content
