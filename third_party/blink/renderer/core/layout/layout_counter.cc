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

#include "base/memory/ptr_util.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_directory_element.h"
#include "third_party/blink/renderer/core/html/html_menu_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

namespace {

String GenerateCounterText(const CounterStyle* counter_style, int value) {
  if (!counter_style) {
    return g_empty_string;
  }
  return counter_style->GenerateRepresentation(value);
}

}  // namespace

LayoutCounter::LayoutCounter(Document& document,
                             const CounterContentData& counter)
    : LayoutText(nullptr, StringImpl::empty_), counter_(counter) {
  SetDocumentForAnonymous(&document);
  View()->AddLayoutCounter();
}

LayoutCounter::~LayoutCounter() = default;

void LayoutCounter::Trace(Visitor* visitor) const {
  visitor->Trace(counter_);
  LayoutText::Trace(visitor);
}

void LayoutCounter::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View()) {
    View()->RemoveLayoutCounter();
  }
  LayoutText::WillBeDestroyed();
}

void LayoutCounter::UpdateCounter(Vector<int> counter_values) {
  NOT_DESTROYED();
  const CounterStyle* counter_style = NullableCounterStyle();
  String text = GenerateCounterText(counter_style, counter_values.front());
  if (!counter_->Separator().IsNull()) {
    for (wtf_size_t i = 1u; i < counter_values.size(); ++i) {
      text = GenerateCounterText(counter_style, counter_values[i]) +
             counter_->Separator() + text;
    }
  }
  SetTextIfNeeded(text);
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
