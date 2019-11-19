/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

AXObjectCache::AXObjectCacheCreateFunction AXObjectCache::create_function_ =
    nullptr;

void AXObjectCache::Init(AXObjectCacheCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

AXObjectCache* AXObjectCache::Create(Document& document) {
  DCHECK(create_function_);
  return create_function_(document);
}

AXObjectCache::AXObjectCache(Document& document)
    : ContextLifecycleObserver(document.GetExecutionContext()) {}

AXObjectCache::~AXObjectCache() = default;

namespace {

typedef HashSet<String, CaseFoldingHash> ARIAWidgetSet;

const char* g_aria_widgets[] = {
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert", "alertdialog", "button", "checkbox", "dialog", "gridcell", "link",
    "log", "marquee", "menuitem", "menuitemcheckbox", "menuitemradio", "option",
    "progressbar", "radio", "scrollbar", "slider", "spinbutton", "status",
    "tab", "tabpanel", "textbox", "timer", "tooltip", "treeitem",
    // Composite user interface widgets.
    // This list is also from the w3.org site referenced above.
    "combobox", "grid", "listbox", "menu", "menubar", "radiogroup", "tablist",
    "tree", "treegrid"};

static ARIAWidgetSet* CreateARIARoleWidgetSet() {
  ARIAWidgetSet* widget_set = new HashSet<String, CaseFoldingHash>();
  for (size_t i = 0; i < base::size(g_aria_widgets); ++i)
    widget_set->insert(String(g_aria_widgets[i]));
  return widget_set;
}

bool IncludesARIAWidgetRole(const String& role) {
  static const HashSet<String, CaseFoldingHash>* role_set =
      CreateARIARoleWidgetSet();

  Vector<String> role_vector;
  role.Split(' ', role_vector);
  for (const auto& child : role_vector) {
    if (role_set->Contains(child))
      return true;
  }
  return false;
}

const char* g_aria_interactive_widget_attributes[] = {
    // These attributes implicitly indicate the given widget is interactive.
    // From http://www.w3.org/TR/wai-aria/states_and_properties#attrs_widgets
    // clang-format off
    "aria-activedescendant",
    "aria-checked",
    "aria-controls",
    "aria-disabled",  // If it's disabled, it can be made interactive.
    "aria-haspopup",
    "aria-multiselectable",
    "aria-required",
    "aria-selected"
    // clang-format on
};

bool HasInteractiveARIAAttribute(const Element& element) {
  for (size_t i = 0; i < base::size(g_aria_interactive_widget_attributes);
       ++i) {
    const char* attribute = g_aria_interactive_widget_attributes[i];
    if (element.hasAttribute(attribute)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool AXObjectCache::IsInsideFocusableElementOrARIAWidget(const Node& node) {
  const Node* cur_node = &node;
  do {
    if (const auto* element = DynamicTo<Element>(cur_node)) {
      if (element->IsFocusable())
        return true;
      String role = element->getAttribute("role");
      if (!role.IsEmpty() && IncludesARIAWidgetRole(role))
        return true;
      if (HasInteractiveARIAAttribute(*element))
        return true;
    }
    cur_node = cur_node->parentNode();
  } while (cur_node && !IsA<HTMLBodyElement>(node));
  return false;
}

void AXObjectCache::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
