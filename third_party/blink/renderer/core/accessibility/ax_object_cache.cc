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
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/accessibility/ax_error_types.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"

namespace blink {

namespace {

constexpr char kAXTreeSnapshotterErrorHistogramName[] =
    "Accessibility.AXTreeSnapshotter.Snapshot.Error";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AXTreeSnapshotErrorReason)
enum class AXTreeSnapshotErrorReason {
  kGenericSerializationError = 0,
  kNoWebFrame = 1,
  kNoActiveDocument = 2,
  kNoExistingAXObjectCache = 3,
  kSerializeMaxNodesReached = 4,
  kSerializeTimeoutReached = 5,
  kSerializeMaxNodesAndTimeoutReached = 6,
  kMaxValue = kSerializeMaxNodesAndTimeoutReached,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:AXTreeSnapshotErrorReason)

}  // namespace

AXObjectCache::AXObjectCacheCreateFunction AXObjectCache::create_function_ =
    nullptr;

void AXObjectCache::Init(AXObjectCacheCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

AXObjectCache* AXObjectCache::Create(Document& document,
                                     const ui::AXMode& ax_mode) {
  DCHECK(create_function_);
  return create_function_(document, ax_mode, /*for_snapshot_only*/ false);
}

AXObjectCache* AXObjectCache::CreateSnapshotter(Document& document,
                                                const ui::AXMode& ax_mode) {
  DCHECK(create_function_);
  return create_function_(document, ax_mode, /*for_snapshot_only*/ true);
}

void AXObjectCache::SnapshotAccessibilityTree(Document* document,
                                              size_t max_nodes,
                                              base::TimeDelta timeout,
                                              ui::AXTreeUpdate* response,
                                              const ui::AXMode& ax_mode) {
  base::UmaHistogramBoolean("Accessibility.AXTreeSnapshotter.Snapshot.Request",
                            true);

  if (!document || !document->IsActive()) {
    base::UmaHistogramEnumeration(kAXTreeSnapshotterErrorHistogramName,
                                  AXTreeSnapshotErrorReason::kNoActiveDocument);
    return;
  }

  // Do not generate inline textboxes, which are expensive to create and just
  // present extra noise to snapshot consumers.
  ui::AXMode snapshot_mode(ax_mode);
  snapshot_mode.set_mode(ui::AXMode::kInlineTextBoxes, false);

  ui::AXSerializationErrorFlagSet out_error;
  // This creates a different AXObjectCache from any owned by document for the
  // case where a11y stays on, because the AXMode may require a different set
  // of nodes.
  AXObjectCache* cache = CreateSnapshotter(*document, snapshot_mode);
  cache->SerializeEntireTreeAndDispose(max_nodes, timeout, response,
                                       &out_error);

  bool max_nodes_reached =
      out_error.Has(ui::AXSerializationErrorFlag::kMaxNodesReached);
  bool timeout_reached =
      out_error.Has(ui::AXSerializationErrorFlag::kTimeoutReached);
  if (max_nodes_reached && timeout_reached) {
    base::UmaHistogramEnumeration(
        kAXTreeSnapshotterErrorHistogramName,
        AXTreeSnapshotErrorReason::kSerializeMaxNodesAndTimeoutReached);
  } else if (max_nodes_reached) {
    base::UmaHistogramEnumeration(
        kAXTreeSnapshotterErrorHistogramName,
        AXTreeSnapshotErrorReason::kSerializeMaxNodesReached);
  } else if (timeout_reached) {
    base::UmaHistogramEnumeration(
        kAXTreeSnapshotterErrorHistogramName,
        AXTreeSnapshotErrorReason::kSerializeTimeoutReached);
  }
}

namespace {

using ARIAWidgetSet = HashSet<String, DeprecatedCaseFoldingHashTraits<String>>;

const ARIAWidgetSet& ARIARoleWidgetSet() {
  // clang-format off
  static base::NoDestructor<ARIAWidgetSet> widget_set({
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert", "alertdialog", "button", "checkbox", "dialog", "gridcell", "link",
    "log", "marquee", "menuitem", "menuitemcheckbox", "menuitemradio", "option",
    "progressbar", "radio", "scrollbar", "slider", "spinbutton", "status",
    "tab", "tabpanel", "textbox", "timer", "tooltip", "treeitem",
    // Composite user interface widgets.
    // This list is also from the w3.org site referenced above.
    "combobox", "grid", "listbox", "menu", "menubar", "radiogroup", "tablist",
    "tree", "treegrid",
  });
  // clang-format on
  return *widget_set;
}

bool IncludesARIAWidgetRole(const String& role) {
  const ARIAWidgetSet& role_set = ARIARoleWidgetSet();
  Vector<String> role_vector = role.SplitSkippingEmpty(' ');
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
      &html_names::kAriaActionsAttr,
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
      if (element->IsFocusable()) {
        return true;
      }
      String role = element->getAttribute(html_names::kRoleAttr);
      if (!role.empty() && IncludesARIAWidgetRole(role)) {
        return true;
      }
      if (HasInteractiveARIAAttribute(*element)) {
        return true;
      }
    }
    cur_node = cur_node->parentNode();
  } while (cur_node && !IsA<HTMLBodyElement>(cur_node));
  return false;
}

}  // namespace blink
