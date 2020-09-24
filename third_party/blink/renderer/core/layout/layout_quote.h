/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_QUOTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_QUOTE_H_

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/style/quotes_data.h"

namespace blink {

class LayoutTextFragment;
class PseudoElement;

// LayoutQuote is the layout object associated with generated quotes
// ("content: open-quote | close-quote | no-open-quote | no-close-quote").
// http://www.w3.org/TR/CSS2/generate.html#quotes-insert
//
// This object is generated thus always anonymous.
//
// For performance reasons, LayoutQuotes form a doubly-linked list. See |m_next|
// and |m_previous| below.
class LayoutQuote final : public LayoutInline {
 public:
  LayoutQuote(PseudoElement&, const QuoteType);
  ~LayoutQuote() override;
  void AttachQuote();

  const char* GetName() const override { return "LayoutQuote"; }

 private:
  void DetachQuote();

  void WillBeDestroyed() override;
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectQuote || LayoutInline::IsOfType(type);
  }
  void StyleDidChange(StyleDifference, const ComputedStyle*) override;
  void WillBeRemovedFromTree() override;

  String ComputeText() const;
  void UpdateText();
  const QuotesData* GetQuotesData() const;
  void UpdateDepth();
  bool IsAttached() { return attached_; }

  LayoutTextFragment* FindFragmentChild() const;

  // Type of this LayoutQuote: open-quote, close-quote, no-open-quote,
  // no-close-quote.
  QuoteType type_;

  // Number of open quotes in the tree. Also called the nesting level
  // in CSS 2.1.
  // Used to determine if a LayoutQuote is invalid (closing quote without a
  // matching opening quote) and which quote character to use (see the 'quote'
  // property that is used to define quote character pairs).
  int depth_;

  // The next and previous LayoutQuote in layout tree order.
  // LayoutQuotes are linked together by this doubly-linked list.
  // Those are used to compute |m_depth| in an efficient manner.
  LayoutQuote* next_;
  LayoutQuote* previous_;

  // The pseudo-element that owns us.
  //
  // Lifetime is the same as LayoutObject::m_node, so this is safe.
  UntracedMember<PseudoElement> owning_pseudo_;

  // This tracks whether this LayoutQuote was inserted into the layout tree
  // and its position in the linked list is correct (m_next and m_previous).
  // It's used for both performance (avoid unneeded tree walks to find the
  // previous and next quotes) and conformance (|m_depth| relies on an
  // up-to-date linked list positions).
  bool attached_;

  // Cached text for this quote.
  String text_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutQuote, IsQuote());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_QUOTE_H_
