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
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

AXObjectCache::AXObjectCacheCreateFunction AXObjectCache::create_function_ =
    nullptr;

void AXObjectCache::Init(AXObjectCacheCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

AXObjectCache* AXObjectCache::Create(Document& document,
                                     const ui::AXMode& ax_mode) {
  DCHECK(create_function_);
  return create_function_(document, ax_mode);
}

namespace {

using ARIAWidgetSet = HashSet<String, CaseFoldingHashTraits<String>>;

const ARIAWidgetSet& ARIARoleWidgetSet() {
  // clang-format off
  DEFINE_STATIC_LOCAL(ARIAWidgetSet, widget_set, ({
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert", "alertdialog", "button", "checkbox", "dialog", "gridcell", "link",
    "log", "marquee", "menuitem", "menuitemcheckbox", "menuitemradio", "option",
    "progressbar", "radio", "scrollbar", "slider", "spinbutton", "status",
    "tab", "tabpanel", "textbox", "timer", "tooltip", "treeitem",
    // Composite user interface widgets.
    // This list is also from the w3.org site referenced above.
    "combobox", "grid", "listbox", "menu", "menubar", "radiogroup", "tablist",
    "tree", "treegrid",
  }));
  // clang-format on
  return widget_set;
}

bool IncludesARIAWidgetRole(const String& role) {
  const ARIAWidgetSet& role_set = ARIARoleWidgetSet();
  Vector<String> role_vector;
  role.Split(' ', role_vector);
  for (const auto& child : role_vector) {
    if (role_set.Contains(child)) {
      return true;
    }
  }
  return false;
}

bool HasInteractiveARIAAttribute(const Element& element) {
  static const QualifiedName* aria_interactive_widget_attributes[] = {
      // These attributes implicitly indicate the given widget is interactive.
      // From http://www.w3.org/TR/wai-aria/states_and_properties#attrs_widgets
      // clang-format off
      &html_names::kAriaActivedescendantAttr,
      &html_names::kAriaCheckedAttr,
      &html_names::kAriaControlsAttr,
      // If it's disabled, it can be made interactive.
      &html_names::kAriaDisabledAttr,
      &html_names::kAriaHaspopupAttr,
      &html_names::kAriaMultiselectableAttr,
      &html_names::kAriaRequiredAttr,
      &html_names::kAriaSelectedAttr
      // clang-format on
  };

  for (const auto* attribute : aria_interactive_widget_attributes) {
    if (element.hasAttribute(*attribute)) {
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
      String role = element->getAttribute(html_names::kRoleAttr);
      if (!role.empty() && IncludesARIAWidgetRole(role))
        return true;
      if (HasInteractiveARIAAttribute(*element))
        return true;
    }
    cur_node = cur_node->parentNode();
  } while (cur_node && !IsA<HTMLBodyElement>(node));
  return false;
}

}  // namespace blink
