/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OLIST_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OLIST_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/counter_directives.h"

namespace blink {

class HTMLOListElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLOListElement(Document&);

  int64_t InitialCounter() const {
    if (!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled()) {
      return HasExplicitStart()
                 ? ExplicitStart()
                 : (IsReversed() ? InitialCounterForReversed() : 1);
    }
    if (HasExplicitStart()) {
      return ExplicitStart();
    }
    return IsReversed() ? InitialCounterForReversed() : 0;
  }
  int start() const { return has_explicit_start_ ? start_ : 1; }
  void setStart(int);

  bool IsReversed() const {
    if (RuntimeEnabledFeatures::CSSCounterResetReversedEnabled()) {
      return ListItemCounterDirectives().IsResetReversed();
    }
    return is_reversed_;
  }

  void ItemCountChanged() { should_recalculate_initial_counter_ = true; }

 private:
  void InvalidateItemValues();

  int InitialCounterForReversed() const {
    DCHECK(!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() ||
           IsReversed());
    if (should_recalculate_initial_counter_) {
      const_cast<HTMLOListElement*>(this)
          ->RecalculateInitialCounterForReversed();
    }
    return initial_counter_for_reversed_;
  }

  void RecalculateInitialCounterForReversed();

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      HeapVector<CSSPropertyValue, 8>&) override;

  const CounterDirectives ListItemCounterDirectives() const {
    if (const ComputedStyle* style = GetComputedStyle()) {
      return style->GetCounterDirectives(AtomicString("list-item"));
    }
    return CounterDirectives();
  }
  bool HasExplicitStart() const {
    if (RuntimeEnabledFeatures::CSSCounterResetReversedEnabled()) {
      return ListItemCounterDirectives().ResetValueInt64().has_value();
    }
    return has_explicit_start_;
  }
  int64_t ExplicitStart() const {
    if (RuntimeEnabledFeatures::CSSCounterResetReversedEnabled()) {
      DCHECK(HasExplicitStart());
      return ListItemCounterDirectives().ResetValueInt64().value();
    }
    return start_;
  }

  // These values are used only for DOM reflection of start attribution, not
  // used for the calculation of initial counter.
  int start_ = 0xBADBEEF;
  bool has_explicit_start_ : 1 = false;

  // TODO(crbug.com/40760770): Remove this value when
  // CSSListCounterAccounting is enabled by default and removed.
  bool is_reversed_ : 1 = false;

  int initial_counter_for_reversed_ = 0;

  bool should_recalculate_initial_counter_ : 1 = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OLIST_ELEMENT_H_
