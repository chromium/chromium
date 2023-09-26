/**
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_counter.h"

#include <memory>

#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/counters_scope_tree.h"
#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

String GenerateCounterText(const CounterStyle* counter_style, int value) {
  if (!counter_style) {
    return g_empty_string;
  }
  return counter_style->GenerateRepresentation(value);
}

CounterNode* GetNextCounter(const CounterNode& counter,
                            const WTF::AtomicString& identifier) {
  if (CountersScope* parent_scope = counter.Scope()->Parent()) {
    return &parent_scope->FirstCounter();
  }
  // Leave the style scope.
  StyleContainmentScope* parent_style_scope =
      counter.Scope()->StyleScope()->Parent();
  if (parent_style_scope) {
    CountersScope* scope_in_parent =
        parent_style_scope->FindCountersScopeForElement(counter.OwnerElement(),
                                                        identifier);
    // Find the counters scope in parent style scope.
    if (scope_in_parent) {
      return &scope_in_parent->FirstCounter();
    }
  }
  return nullptr;
}

bool IsCounterDecendantOf(const CounterNode& decendant,
                          const CounterNode& ancestor) {
  return decendant.OwnerElement().IsDescendantOf(&ancestor.OwnerElement()) ||
         decendant.OwnerElement() == ancestor.OwnerElement();
}

}  // namespace

LayoutCounter::LayoutCounter(PseudoElement& pseudo,
                             const CounterContentData& counter)
    : LayoutText(nullptr, StringImpl::empty_),
      counter_(counter),
      counter_node_(nullptr) {
  SetDocumentForAnonymous(&pseudo.GetDocument());
}

LayoutCounter::~LayoutCounter() = default;

void LayoutCounter::Trace(Visitor* visitor) const {
  visitor->Trace(counter_);
  visitor->Trace(counter_node_);
  LayoutText::Trace(visitor);
}

void LayoutCounter::WillBeDestroyed() {
  NOT_DESTROYED();
  if (counter_node_) {
    if (CountersScope* scope = counter_node_->Scope()) {
      // When we remove the counter, we need to mark the style
      // containment scope it belongs to as dirty.
      GetDocument()
          .GetStyleEngine()
          .EnsureStyleContainmentScopeTree()
          .UpdateOutermostCountersDirtyScope(scope->StyleScope());
      scope->StyleScope()
          ->GetCountersScopeTree()
          ->RemoveCounterForLayoutCounter(*this);
    }
  }
  LayoutText::WillBeDestroyed();
}

String LayoutCounter::OriginalText() const {
  NOT_DESTROYED();
  if (!counter_node_) {
    return GenerateCounterText(NullableCounterStyle(), 0);
  }
  int value = counter_node_->ValueAfter();
  const CounterStyle* counter_style = NullableCounterStyle();
  String text = GenerateCounterText(counter_style, value);
  // If the separator exists, we need to append all of the parent values as
  // well, including the ones that cross the style containment boundary.
  if (!counter_->Separator().IsNull()) {
    const CounterNode* counter = &counter_node_->Scope()->FirstCounter();
    if (counter == counter_node_ ||
        (!IsCounterDecendantOf(*counter_node_, *counter) &&
         counter->Scope()->Parent())) {
      counter = GetNextCounter(*counter, counter_->Identifier());
    }
    while (counter) {
      // If we have parent in current style scope.
      const CounterNode* next_counter =
          GetNextCounter(*counter, counter_->Identifier());
      if (next_counter) {
        text = GenerateCounterText(counter_style, counter->ValueBefore()) +
               counter_->Separator() + text;
      }
      counter = next_counter;
    }
  }
  return text;
}

void LayoutCounter::UpdateCounter() {
  NOT_DESTROYED();
  SetTextIfNeeded(OriginalText());
}

const CounterStyle* LayoutCounter::NullableCounterStyle() const {
  // Note: CSS3 spec doesn't allow 'none' but CSS2.1 allows it. We currently
  // allow it for backward compatibility.
  // See https://github.com/w3c/csswg-drafts/issues/5795 for details.
  if (counter_->ListStyle() == "none") {
    return nullptr;
  }
  return &GetDocument().GetStyleEngine().FindCounterStyleAcrossScopes(
      counter_->ListStyle(), counter_->GetTreeScope());
}

bool LayoutCounter::IsDirectionalSymbolMarker() const {
  const auto* counter_style = NullableCounterStyle();
  if (!counter_style || !counter_style->IsPredefinedSymbolMarker()) {
    return false;
  }
  const AtomicString& list_style = counter_->ListStyle();
  return list_style == keywords::kDisclosureOpen ||
         list_style == keywords::kDisclosureClosed;
}

const AtomicString& LayoutCounter::Separator() const {
  return counter_->Separator();
}

// static
const AtomicString& LayoutCounter::ListStyle(const LayoutObject* object,
                                             const ComputedStyle& style) {
  if (const auto* counter = DynamicTo<LayoutCounter>(object)) {
    return counter->counter_->ListStyle();
  }
  return style.ListStyleType()->GetCounterStyleName();
}

}  // namespace blink
