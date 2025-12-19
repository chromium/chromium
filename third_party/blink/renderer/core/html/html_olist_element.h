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

namespace blink {

class HTMLOListElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLOListElement(Document&);

  int64_t InitialCounter() const {
    if (!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled()) {
      return has_explicit_start_
                 ? start_
                 : (is_reversed_ ? InitialCounterForReversed() : 1);
    }
    if (has_explicit_start_) {
      // We don't apply a counter value of the list item if we have an explicit
      // start value.
      return static_cast<int64_t>(start_) + (is_reversed_ ? 1 : -1);
    }
    return is_reversed_ ? InitialCounterForReversed() : 0;
  }
  int start() const { return has_explicit_start_ ? start_ : 1; }
  void setStart(int);

  bool IsReversed() const { return is_reversed_; }

  void ItemCountChanged() { should_recalculate_initial_counter_ = true; }

 private:
  void InvalidateItemValues();

  int InitialCounterForReversed() const {
    DCHECK(!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() ||
           is_reversed_);
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

  int start_ = 0xBADBEEF;
  int initial_counter_for_reversed_ = 0;

  bool has_explicit_start_ : 1 = false;
  bool is_reversed_ : 1 = false;
  bool should_recalculate_initial_counter_ : 1 = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OLIST_ELEMENT_H_
