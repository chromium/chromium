// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_win.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_uia_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/base/win/atl_module.h"
#include "ui/gfx/win/hwnd_util.h"

namespace content {

using ui::AXFormatValue;
using ui::IAccessible2RoleToString;
using ui::IAccessible2StateToStringVector;
using ui::IAccessibleStateToStringVector;

void AccessibilityTreeFormatterWin::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  // Too noisy: HOTTRACKED, LINKED, SELECTABLE, IA2_STATE_EDITABLE,
  //            IA2_STATE_OPAQUE, IA2_STATE_SELECTAbLE_TEXT,
  //            IA2_STATE_SINGLE_LINE, IA2_STATE_VERTICAL.
  // Too unpredictiable: OFFSCREEN
  // Windows states to log by default:
  AddPropertyFilter(property_filters, "ALERT*");
  AddPropertyFilter(property_filters, "ANIMATED*");
  AddPropertyFilter(property_filters, "BUSY");
  AddPropertyFilter(property_filters, "CHECKED");
  AddPropertyFilter(property_filters, "COLLAPSED");
  AddPropertyFilter(property_filters, "EXPANDED");
  AddPropertyFilter(property_filters, "FLOATING");
  AddPropertyFilter(property_filters, "FOCUSABLE");
  AddPropertyFilter(property_filters, "HASPOPUP");
  AddPropertyFilter(property_filters, "INVISIBLE");
  AddPropertyFilter(property_filters, "MARQUEED");
  AddPropertyFilter(property_filters, "MIXED");
  AddPropertyFilter(property_filters, "MOVEABLE");
  AddPropertyFilter(property_filters, "MULTISELECTABLE");
  AddPropertyFilter(property_filters, "PRESSED");
  AddPropertyFilter(property_filters, "PROTECTED");
  AddPropertyFilter(property_filters, "READONLY");
  AddPropertyFilter(property_filters, "SELECTED");
  AddPropertyFilter(property_filters, "SIZEABLE");
  AddPropertyFilter(property_filters, "TRAVERSED");
  AddPropertyFilter(property_filters, "UNAVAILABLE");
  AddPropertyFilter(property_filters, "IA2_STATE_ACTIVE");
  AddPropertyFilter(property_filters, "IA2_STATE_ARMED");
  AddPropertyFilter(property_filters, "IA2_STATE_CHECKABLE");
  AddPropertyFilter(property_filters, "IA2_STATE_DEFUNCT");
  AddPropertyFilter(property_filters, "IA2_STATE_HORIZONTAL");
  AddPropertyFilter(property_filters, "IA2_STATE_ICONIFIED");
  AddPropertyFilter(property_filters, "IA2_STATE_INVALID_ENTRY");
  AddPropertyFilter(property_filters, "IA2_STATE_MODAL");
  AddPropertyFilter(property_filters, "IA2_STATE_MULTI_LINE");
  AddPropertyFilter(property_filters, "IA2_STATE_PINNED");
  AddPropertyFilter(property_filters, "IA2_STATE_REQUIRED");
  AddPropertyFilter(property_filters, "IA2_STATE_STALE");
  AddPropertyFilter(property_filters, "IA2_STATE_TRANSIENT");
  // Reduce flakiness.
  AddPropertyFilter(property_filters, "FOCUSED", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "HOTTRACKED", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "OFFSCREEN", AXPropertyFilter::DENY);
}

AccessibilityTreeFormatterWin::AccessibilityTreeFormatterWin() {
  ui::win::CreateATLModuleIfNeeded();
}

AccessibilityTreeFormatterWin::~AccessibilityTreeFormatterWin() {}

static HRESULT QuerySimpleDOMNode(IAccessible* accessible,
                                  ISimpleDOMNode** simple_dom_node) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_ISimpleDOMNode, simple_dom_node);
}

static HRESULT QueryIAccessible2(IAccessible* accessible,
                                 IAccessible2** accessible2) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessible2, accessible2);
}

static HRESULT QueryIAccessibleAction(IAccessible* accessible,
                                      IAccessibleAction** accessibleAction) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;

  return service_provider->QueryService(IID_IAccessibleAction,
                                        accessibleAction);
}

static HRESULT QueryIAccessibleHypertext(
    IAccessible* accessible,
    IAccessibleHypertext** accessibleHypertext) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleHypertext,
                                        accessibleHypertext);
}

static HRESULT QueryIAccessibleTable(IAccessible* accessible,
                                     IAccessibleTable** accessibleTable) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleTable, accessibleTable);
}

static HRESULT QueryIAccessibleTableCell(
    IAccessible* accessible,
    IAccessibleTableCell** accessibleTableCell) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleTableCell,
                                        accessibleTableCell);
}

static HRESULT QueryIAccessibleText(IAccessible* accessible,
                                    IAccessibleText** accessibleText) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleText, accessibleText);
}

static HRESULT QueryIAccessibleValue(IAccessible* accessible,
                                     IAccessibleValue** accessibleValue) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleValue, accessibleValue);
}

Microsoft::WRL::ComPtr<IAccessible>
GetIAObject(ui::AXPlatformNodeDelegate* node, LONG& root_x, LONG& root_y) {
  DCHECK(node);
  BrowserAccessibility* node_internal =
      BrowserAccessibility::FromAXPlatformNodeDelegate(node);
  DCHECK(node_internal);
  BrowserAccessibilityManager* root_manager =
      node_internal->manager()->GetRootManager();
  DCHECK(root_manager);

  base::win::ScopedVariant variant_self(CHILDID_SELF);
  LONG root_width, root_height;
  BrowserAccessibility* root = root_manager->GetRoot();
  HRESULT hr = ToBrowserAccessibilityWin(root)->GetCOM()->accLocation(
      &root_x, &root_y, &root_width, &root_height, variant_self);
  DCHECK(SUCCEEDED(hr));

  return ToBrowserAccessibilityComWin(node_internal);
}

base::Value AccessibilityTreeFormatterWin::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  LONG root_x = 0, root_y = 0;
  Microsoft::WRL::ComPtr<IAccessible> node_ia =
      GetIAObject(node, root_x, root_y);

  base::DictionaryValue dict;
  AddProperties(node_ia, &dict, root_x, root_y);
  return std::move(dict);
}

base::Value AccessibilityTreeFormatterWin::BuildTree(
    ui::AXPlatformNodeDelegate* start) const {
  LONG root_x = 0, root_y = 0;
  Microsoft::WRL::ComPtr<IAccessible> start_ia =
      GetIAObject(start, root_x, root_y);

  base::Value dict(base::Value::Type::DICTIONARY);
  RecursiveBuildTree(start_ia, &dict, root_x, root_y);
  return dict;
}

base::Value AccessibilityTreeFormatterWin::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  base::Value dict(base::Value::Type::DICTIONARY);

  HWND hwnd = GetHWNDBySelector(selector);
  if (!hwnd)
    return dict;

  Microsoft::WRL::ComPtr<IAccessible> root;
  HRESULT hr =
      ::AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, IID_PPV_ARGS(&root));
  if (FAILED(hr))
    return dict;

  if (selector.types & AXTreeSelector::ActiveTab) {
    root = FindActiveDocument(root.Get());
    if (!root) {
      return dict;
    }
  }

  RecursiveBuildTree(root, &dict, 0, 0);
  return dict;
}

void AccessibilityTreeFormatterWin::RecursiveBuildTree(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict,
    LONG root_x,
    LONG root_y) const {
  ui::AXPlatformNode* platform_node =
      ui::AXPlatformNode::FromNativeViewAccessible(node.Get());

  bool skipChildren = false;
  if (platform_node) {
    ui::AXPlatformNodeDelegate* delegate = platform_node->GetDelegate();
    DCHECK(delegate);

    if (!ShouldDumpNode(*delegate))
      return;

    if (!ShouldDumpChildren(*delegate))
      skipChildren = true;
  }

  AddProperties(node, dict, root_x, root_y);
  if (skipChildren)
    return;

  base::Value child_list(base::Value::Type::LIST);
  for (const ui::MSAAChild& msaa_child : ui::MSAAChildren(node)) {
    base::Value child_dict(base::Value::Type::DICTIONARY);

    Microsoft::WRL::ComPtr<IAccessible> child = msaa_child.AsIAccessible();
    if (child) {
      RecursiveBuildTree(child, &child_dict, root_x, root_y);
    } else {
      const base::win::ScopedVariant& child_variant = msaa_child.AsVariant();
      if (child_variant.type() == VT_EMPTY ||
          child_variant.type() == VT_DISPATCH) {
        child_dict.SetStringPath("error", "[Error retrieving child]");
      } else if (child_variant.type() == VT_I4) {
        // Partial child does not have its own object.
        // Add minimal info -- role and name.
        base::win::ScopedVariant role_variant;
        if (SUCCEEDED(
                node->get_accRole(child_variant, role_variant.Receive()))) {
          if (role_variant.type() == VT_I4) {
            child_dict.SetStringPath("role", " [partial child]");
          }
        }
        base::win::ScopedBstr name;
        if (S_OK == node->get_accName(child_variant, name.Receive())) {
          child_dict.SetStringPath("name", base::WideToUTF8(name.Get()));
        }
      } else {
        child_dict.SetStringPath("error", "[Unknown child type]");
      }
    }
    child_list.Append(std::move(child_dict));
  }
  dict->SetPath(kChildrenDictAttr, std::move(child_list));
}

const char* const ALL_ATTRIBUTES[] = {
    "name",
    "parent",
    "window_class",
    "value",
    "states",
    "attributes",
    "text_attributes",
    "ia2_hypertext",
    "currentValue",
    "minimumValue",
    "maximumValue",
    "description",
    "default_action",
    "action_name",
    "keyboard_shortcut",
    "location",
    "size",
    "index_in_parent",
    "n_relations",
    "group_level",
    "similar_items_in_group",
    "position_in_group",
    "table_rows",
    "table_columns",
    "row_index",
    "column_index",
    "row_headers",
    "column_headers",
    "n_characters",
    "caret_offset",
    "n_selections",
    "selection_start",
    "selection_end",
    "localized_extended_role",
    "inner_html",
    "ia2_table_cell_column_index",
    "ia2_table_cell_row_index",
};

void AccessibilityTreeFormatterWin::AddProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict,
    LONG root_x,
    LONG root_y) const {
  AddMSAAProperties(node, dict, root_x, root_y);
  AddSimpleDOMNodeProperties(node, dict);
  if (AddIA2Properties(node, dict)) {
    AddIA2ActionProperties(node, dict);
    AddIA2HypertextProperties(node, dict);
    AddIA2TableProperties(node, dict);
    AddIA2TableCellProperties(node, dict);
    AddIA2TextProperties(node, dict);
    AddIA2ValueProperties(node, dict);
  }
}

std::u16string RoleVariantToString(const base::win::ScopedVariant& role) {
  if (role.type() == VT_I4)
    return base::WideToUTF16(IAccessible2RoleToString(V_I4(role.ptr())));
  if (role.type() == VT_BSTR)
    return base::WideToUTF16(V_BSTR(role.ptr()));
  return std::u16string();
}

void AccessibilityTreeFormatterWin::AddMSAAProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict,
    LONG root_x,
    LONG root_y) const {
  base::win::ScopedVariant variant_self(CHILDID_SELF);
  base::win::ScopedBstr bstr;
  base::win::ScopedVariant ia_role_variant;
  if (SUCCEEDED(node->get_accRole(variant_self, ia_role_variant.Receive()))) {
    dict->SetStringPath("role", RoleVariantToString(ia_role_variant));
  }

  // If S_FALSE it means there is no name
  if (S_OK == node->get_accName(variant_self, bstr.Receive())) {
    dict->SetStringPath("name", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  Microsoft::WRL::ComPtr<IDispatch> parent_dispatch;
  if (SUCCEEDED(node->get_accParent(&parent_dispatch))) {
    Microsoft::WRL::ComPtr<IAccessible> parent_accessible;
    if (!parent_dispatch) {
      dict->SetStringPath("parent", "[null]");
    } else if (SUCCEEDED(parent_dispatch.As(&parent_accessible))) {
      base::win::ScopedVariant parent_ia_role_variant;
      if (SUCCEEDED(parent_accessible->get_accRole(
              variant_self, parent_ia_role_variant.Receive())))
        dict->SetStringPath("parent",
                            RoleVariantToString(parent_ia_role_variant));
      else
        dict->SetStringPath("parent", "[Error retrieving role from parent]");
    } else {
      dict->SetStringPath("parent", "[Error getting IAccessible* for parent]");
    }
  } else {
    dict->SetStringPath("parent", "[Error retrieving parent]");
  }

  HWND hwnd;
  if (SUCCEEDED(::WindowFromAccessibleObject(node.Get(), &hwnd)) && hwnd) {
    dict->SetStringPath("window_class",
                        base::WideToUTF16(gfx::GetClassName(hwnd)));
  } else {
    // This method is implemented by oleacc.dll and uses get_accParent,
    // therefore it Will fail if get_accParent from root fails.
    dict->SetStringPath("window_class", "[Error]");
  }

  if (SUCCEEDED(node->get_accValue(variant_self, bstr.Receive())) && bstr.Get())
    dict->SetStringPath("value", base::WideToUTF8(bstr.Get()));
  bstr.Reset();

  int32_t ia_state = 0;
  base::win::ScopedVariant ia_state_variant;
  if (node->get_accState(variant_self, ia_state_variant.Receive()) == S_OK &&
      ia_state_variant.type() == VT_I4) {
    ia_state = ia_state_variant.ptr()->intVal;
    std::vector<std::wstring> state_strings;
    IAccessibleStateToStringVector(ia_state, &state_strings);

    base::Value::ListStorage states;
    states.reserve(state_strings.size());
    for (const auto& str : state_strings)
      states.emplace_back(base::WideToUTF8(str));
    dict->SetPath("states", base::Value(std::move(states)));
  }

  if (S_OK == node->get_accDescription(variant_self, bstr.Receive())) {
    dict->SetStringPath("description", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  // |get_accDefaultAction| returns a localized string.
  if (S_OK == node->get_accDefaultAction(variant_self, bstr.Receive())) {
    dict->SetStringPath("default_action", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  if (S_OK == node->get_accKeyboardShortcut(variant_self, bstr.Receive())) {
    dict->SetStringPath("keyboard_shortcut", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  if (S_OK == node->get_accHelp(variant_self, bstr.Receive())) {
    dict->SetStringPath("help", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  LONG x, y, width, height;
  if (SUCCEEDED(node->accLocation(&x, &y, &width, &height, variant_self))) {
    base::Value location(base::Value::Type::DICTIONARY);
    location.SetIntPath("x", x - root_x);
    location.SetIntPath("y", y - root_y);
    dict->SetPath("location", std::move(location));

    base::Value size(base::Value::Type::DICTIONARY);
    size.SetIntPath("width", width);
    size.SetIntPath("height", height);
    dict->SetPath("size", std::move(size));
  }
}

void AccessibilityTreeFormatterWin::AddSimpleDOMNodeProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<ISimpleDOMNode> simple_dom_node;

  if (S_OK != QuerySimpleDOMNode(node.Get(), &simple_dom_node))
    return;  // No IA2Value, we are finished with this node.

  base::win::ScopedBstr bstr;
  if (SUCCEEDED(simple_dom_node->get_innerHTML(bstr.Receive()))) {
    dict->SetStringPath("inner_html", base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();
}

bool AccessibilityTreeFormatterWin::AddIA2Properties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessible2> ia2;
  if (S_OK != QueryIAccessible2(node.Get(), &ia2))
    return false;  // No IA2, we are finished with this node.

  LONG ia2_role = 0;
  if (SUCCEEDED(ia2->role(&ia2_role))) {
    const std::string* legacy_role = dict->FindStringPath("role");
    if (legacy_role)
      dict->SetStringPath("msaa_legacy_role", *legacy_role);
    // Overwrite MSAA role which is more limited.
    dict->SetStringPath("role",
                        base::WideToUTF8(IAccessible2RoleToString(ia2_role)));
  }

  std::vector<std::wstring> state_strings;
  AccessibleStates states;
  if (ia2->get_states(&states) == S_OK) {
    IAccessible2StateToStringVector(states, &state_strings);
    // Append IA2 state list to MSAA state
    base::Value* states_list = dict->FindListKey("states");
    if (states_list) {
      for (const auto& str : state_strings)
        states_list->Append(base::WideToUTF8(str));
    }
  }

  base::win::ScopedBstr bstr;

  if (ia2->get_attributes(bstr.Receive()) == S_OK) {
    // get_attributes() returns a semicolon delimited string. Turn it into a
    // ListValue
    std::vector<std::u16string> ia2_attributes =
        base::SplitString(base::WideToUTF16(bstr.Get()), std::u16string(1, ';'),
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    base::Value::ListStorage attributes;
    attributes.reserve(ia2_attributes.size());
    for (const auto& str : ia2_attributes)
      attributes.push_back(base::Value(str));
    dict->SetPath("attributes", base::Value(std::move(attributes)));
  }
  bstr.Reset();

  LONG index_in_parent;
  if (SUCCEEDED(ia2->get_indexInParent(&index_in_parent)))
    dict->SetIntPath("index_in_parent", index_in_parent);

  LONG n_relations;
  if (SUCCEEDED(ia2->get_nRelations(&n_relations)))
    dict->SetIntPath("n_relations", n_relations);

  LONG group_level, similar_items_in_group, position_in_group;
  // |GetGroupPosition| returns S_FALSE when no grouping information is
  // available so avoid using |SUCCEEDED|.
  if (ia2->get_groupPosition(&group_level, &similar_items_in_group,
                             &position_in_group) == S_OK) {
    dict->SetIntPath("group_level", group_level);
    dict->SetIntPath("similar_items_in_group", similar_items_in_group);
    dict->SetIntPath("position_in_group", position_in_group);
  }

  if (SUCCEEDED(ia2->get_localizedExtendedRole(bstr.Receive())) && bstr.Get()) {
    dict->SetStringPath("localized_extended_role",
                        base::WideToUTF8(bstr.Get()));
  }
  bstr.Reset();

  return true;
}

void AccessibilityTreeFormatterWin::AddIA2ActionProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleAction> ia2action;
  if (S_OK != QueryIAccessibleAction(node.Get(), &ia2action))
    return;  // No IA2Value, we are finished with this node.

  // |IAccessibleAction::get_name| returns a localized string.
  base::win::ScopedBstr name;
  if (SUCCEEDED(ia2action->get_name(0 /* action_index */, name.Receive())) &&
      name.Get()) {
    dict->SetStringPath("action_name", base::WideToUTF8(name.Get()));
  }
}

void AccessibilityTreeFormatterWin::AddIA2HypertextProperties(
    Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleHypertext> ia2hyper;
  if (S_OK != QueryIAccessibleHypertext(node.Get(), &ia2hyper))
    return;  // No IA2, we are finished with this node

  base::win::ScopedBstr text_bstr;
  HRESULT hr;

  hr = ia2hyper->get_text(0, IA2_TEXT_OFFSET_LENGTH, text_bstr.Receive());
  if (FAILED(hr))
    return;

  std::wstring ia2_hypertext(text_bstr.Get(), text_bstr.Length());
  // IA2 Spec calls embedded objects hyperlinks. We stick to embeds for clarity.
  LONG number_of_embeds;
  hr = ia2hyper->get_nHyperlinks(&number_of_embeds);
  if (SUCCEEDED(hr) && number_of_embeds > 0) {
    // Replace all embedded characters with the child indices of the
    // accessibility objects they refer to.
    std::wstring embedded_character = base::UTF16ToWide(
        std::u16string(1, BrowserAccessibilityComWin::kEmbeddedCharacter));
    size_t character_index = 0;
    size_t hypertext_index = 0;
    while (hypertext_index < ia2_hypertext.length()) {
      if (ia2_hypertext[hypertext_index] !=
          BrowserAccessibilityComWin::kEmbeddedCharacter) {
        ++character_index;
        ++hypertext_index;
        continue;
      }

      LONG index_of_embed;
      hr = ia2hyper->get_hyperlinkIndex(character_index, &index_of_embed);
      // S_FALSE will be returned if no embedded object is found at the given
      // embedded character offset. Exclude child index from such cases.
      LONG child_index = -1;
      if (hr == S_OK) {
        DCHECK_GE(index_of_embed, 0);
        Microsoft::WRL::ComPtr<IAccessibleHyperlink> embedded_object;
        hr = ia2hyper->get_hyperlink(index_of_embed, &embedded_object);
        DCHECK(SUCCEEDED(hr));
        Microsoft::WRL::ComPtr<IAccessible2> ax_embed;
        hr = embedded_object.As(&ax_embed);
        DCHECK(SUCCEEDED(hr));
        hr = ax_embed->get_indexInParent(&child_index);
        DCHECK(SUCCEEDED(hr));
      }

      std::wstring child_index_str(L"<obj");
      if (child_index >= 0) {
        base::StringAppendF(&child_index_str, L"%d>", child_index);
      } else {
        base::StringAppendF(&child_index_str, L">");
      }
      base::ReplaceFirstSubstringAfterOffset(
          &ia2_hypertext, hypertext_index, embedded_character, child_index_str);
      ++character_index;
      hypertext_index += child_index_str.length();
      --number_of_embeds;
    }
  }
  DCHECK_EQ(number_of_embeds, 0);

  dict->SetStringPath("ia2_hypertext", base::WideToUTF16(ia2_hypertext));
}

void AccessibilityTreeFormatterWin::AddIA2TableProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleTable> ia2table;
  if (S_OK != QueryIAccessibleTable(node.Get(), &ia2table))
    return;  // No IA2Text, we are finished with this node.

  LONG table_rows;
  if (SUCCEEDED(ia2table->get_nRows(&table_rows)))
    dict->SetIntPath("table_rows", table_rows);

  LONG table_columns;
  if (SUCCEEDED(ia2table->get_nColumns(&table_columns)))
    dict->SetIntPath("table_columns", table_columns);
}

static std::u16string ProcessAccessiblesArray(IUnknown** accessibles,
                                              LONG num_accessibles) {
  std::u16string related_accessibles_string;
  if (num_accessibles <= 0)
    return related_accessibles_string;

  base::win::ScopedVariant variant_self(CHILDID_SELF);
  for (int index = 0; index < num_accessibles; index++) {
    related_accessibles_string += (index > 0) ? u"," : u"<";
    Microsoft::WRL::ComPtr<IUnknown> unknown = accessibles[index];
    Microsoft::WRL::ComPtr<IAccessible> accessible;
    if (SUCCEEDED(unknown.As(&accessible))) {
      base::win::ScopedBstr name;
      if (S_OK == accessible->get_accName(variant_self, name.Receive()))
        related_accessibles_string += base::WideToUTF16(name.Get());
      else
        related_accessibles_string += u"no name";
    }
  }

  return related_accessibles_string + u">";
}

void AccessibilityTreeFormatterWin::AddIA2TableCellProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleTableCell> ia2cell;
  if (S_OK != QueryIAccessibleTableCell(node.Get(), &ia2cell))
    return;  // No IA2Text, we are finished with this node.

  LONG column_index;
  if (SUCCEEDED(ia2cell->get_columnIndex(&column_index))) {
    dict->SetIntPath("ia2_table_cell_column_index", column_index);
  }

  LONG row_index;
  if (SUCCEEDED(ia2cell->get_rowIndex(&row_index))) {
    dict->SetIntPath("ia2_table_cell_row_index", row_index);
  }

  LONG n_row_header_cells;
  IUnknown** row_headers;
  if (SUCCEEDED(
          ia2cell->get_rowHeaderCells(&row_headers, &n_row_header_cells)) &&
      n_row_header_cells > 0) {
    std::u16string accessibles_desc =
        ProcessAccessiblesArray(row_headers, n_row_header_cells);
    CoTaskMemFree(row_headers);  // Free the array manually.
    dict->SetStringPath("row_headers", accessibles_desc);
  }

  LONG n_column_header_cells;
  IUnknown** column_headers;
  if (SUCCEEDED(ia2cell->get_columnHeaderCells(&column_headers,
                                               &n_column_header_cells)) &&
      n_column_header_cells > 0) {
    std::u16string accessibles_desc =
        ProcessAccessiblesArray(column_headers, n_column_header_cells);
    CoTaskMemFree(column_headers);  // Free the array manually.
    dict->SetStringPath("column_headers", accessibles_desc);
  }
}

void AccessibilityTreeFormatterWin::AddIA2TextProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleText> ia2text;
  if (S_OK != QueryIAccessibleText(node.Get(), &ia2text))
    return;  // No IA2Text, we are finished with this node.

  LONG n_characters;
  if (SUCCEEDED(ia2text->get_nCharacters(&n_characters)))
    dict->SetIntPath("n_characters", n_characters);

  LONG caret_offset;
  if (ia2text->get_caretOffset(&caret_offset) == S_OK)
    dict->SetIntPath("caret_offset", caret_offset);

  LONG n_selections;
  if (SUCCEEDED(ia2text->get_nSelections(&n_selections))) {
    dict->SetIntPath("n_selections", n_selections);
    if (n_selections > 0) {
      LONG start, end;
      if (SUCCEEDED(ia2text->get_selection(0, &start, &end))) {
        dict->SetIntPath("selection_start", start);
        dict->SetIntPath("selection_end", end);
      }
    }
  }

  // Handle IA2 text attributes, adding them as a list.
  // IA2 text attributes comes formatted as a single string, as follows:
  // https://wiki.linuxfoundation.org/accessibility/iaccessible2/textattributes
  base::Value text_attributes(base::Value::Type::LIST);
  LONG current_offset = 0, start_offset, end_offset;
  while (current_offset < n_characters) {
    // TODO(aleventhal) n_characters is not actually useful for ending the
    // loop, because it counts embedded object characters as more than 1,
    // meaning that it counts all the text in the subtree. However, the
    // offsets used in other IAText methods only count the embedded object
    // characters as 1.
    base::win::ScopedBstr temp_bstr;
    HRESULT hr = ia2text->get_attributes(current_offset, &start_offset,
                                         &end_offset, temp_bstr.Receive());
    // The below start_offset < current_offset check is needed because
    // nCharacters is not helpful as described above.
    // When asking for a range past the end of the string, this will occur,
    // although it's not clear whether that's desired or whether
    // S_FALSE or an error should be returned when the offset is out of range.
    if (FAILED(hr) || start_offset < current_offset)
      break;
    // DCHECK(start_offset == current_offset);  // Always at text range start.
    if (hr == S_OK && temp_bstr.Get() && wcslen(temp_bstr.Get())) {
      // Append offset:<number>.
      std::u16string offset_str =
          u"offset:" + base::NumberToString16(start_offset);
      text_attributes.Append(offset_str);
      // Append name:value pairs.
      std::vector<std::wstring> name_val_pairs =
          SplitString(std::wstring(temp_bstr.Get()), L";",
                      base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      for (const auto& name_val_pair : name_val_pairs)
        text_attributes.Append(base::WideToUTF16(name_val_pair));
    }
    current_offset = end_offset;
  }

  dict->SetPath("text_attributes", std::move(text_attributes));
}

void AccessibilityTreeFormatterWin::AddIA2ValueProperties(
    const Microsoft::WRL::ComPtr<IAccessible> node,
    base::Value* dict) const {
  Microsoft::WRL::ComPtr<IAccessibleValue> ia2value;
  if (S_OK != QueryIAccessibleValue(node.Get(), &ia2value))
    return;  // No IA2Value, we are finished with this node.

  base::win::ScopedVariant current_value;
  if (ia2value->get_currentValue(current_value.Receive()) == S_OK &&
      isfinite(V_R8(current_value.ptr()))) {
    dict->SetDoublePath("currentValue", V_R8(current_value.ptr()));
  }

  base::win::ScopedVariant minimum_value;
  if (ia2value->get_minimumValue(minimum_value.Receive()) == S_OK &&
      isfinite(V_R8(minimum_value.ptr()))) {
    dict->SetDoublePath("minimumValue", V_R8(minimum_value.ptr()));
  }

  base::win::ScopedVariant maximum_value;
  if (ia2value->get_maximumValue(maximum_value.Receive()) == S_OK &&
      isfinite(V_R8(maximum_value.ptr()))) {
    dict->SetDoublePath("maximumValue", V_R8(maximum_value.ptr()));
  }
}

std::string AccessibilityTreeFormatterWin::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string line;

  // Always show role, and show it first.
  std::string role_value;
  dict.GetString("role", &role_value);
  WriteAttribute(true, role_value, &line);

  for (const char* attribute_name : ALL_ATTRIBUTES) {
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->type()) {
      case base::Value::Type::LIST: {
        // Currently all list values are string and are written without
        // attribute names.
        std::unique_ptr<base::ListValue> filtered_list(new base::ListValue());

        for (const auto& entry : value->GetListDeprecated()) {
          std::string string_value;
          if (entry.is_string()) {
            if (WriteAttribute(false, entry.GetString(), &line))
              filtered_list->Append(string_value);
          }
        }
        break;
      }
      case base::Value::Type::DICTIONARY: {
        // Currently all dictionary values are coordinates.
        // Revisit this if that changes.
        const base::DictionaryValue* dict_value;
        value->GetAsDictionary(&dict_value);
        if (strcmp(attribute_name, "size") == 0) {
          WriteAttribute(
              false, FormatCoordinates(*dict_value, "size", "width", "height"),
              &line);
        } else if (strcmp(attribute_name, "location") == 0) {
          WriteAttribute(false,
                         FormatCoordinates(*dict_value, "location", "x", "y"),
                         &line);
        }
        break;
      }
      default:
        WriteAttribute(false,
                       base::StringPrintf("%s=%s", attribute_name,
                                          AXFormatValue(*value).c_str()),
                       &line);
        break;
    }
  }

  return line;
}

Microsoft::WRL::ComPtr<IAccessible>
AccessibilityTreeFormatterWin::FindActiveDocument(IAccessible* root) const {
  for (const ui::MSAAChild& child : ui::MSAAChildren(root)) {
    IAccessible* ia = child.AsIAccessible();
    if (!ia)
      continue;

    Microsoft::WRL::ComPtr<IAccessible2> ia2;
    if (FAILED(QueryIAccessible2(ia, &ia2)))
      continue;  // No IA2, we are finished with this node.

    LONG role = 0;
    if (FAILED(ia2->role(&role)))
      continue;

    // Firefox browser exposes documents for all tabs, grab one that doesn't
    // have OFFSCREEN state.
    if (role == IA2_ROLE_INTERNAL_FRAME) {
      base::win::ScopedVariant state_variant;
      if (SUCCEEDED(ia->get_accState(base::win::ScopedVariant(CHILDID_SELF),
                                     state_variant.Receive())) &&
          state_variant.type() == VT_I4) {
        int32_t state = V_I4(state_variant.ptr());
        if (!(state & STATE_SYSTEM_OFFSCREEN))
          return ia;
      }
      continue;
    }

    // Chrome-based browsers expose active tab document only.
    if (role == ROLE_SYSTEM_DOCUMENT)
      return ia;

    Microsoft::WRL::ComPtr<IAccessible> active_document =
        FindActiveDocument(ia);
    if (active_document)
      return active_document;
  }

  return nullptr;
}

}  // namespace content
