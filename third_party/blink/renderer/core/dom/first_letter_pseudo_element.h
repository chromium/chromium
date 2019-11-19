/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FIRST_LETTER_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FIRST_LETTER_PSEUDO_ELEMENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;
class LayoutText;
class LayoutTextFragment;

class CORE_EXPORT FirstLetterPseudoElement final : public PseudoElement {
 public:
  explicit FirstLetterPseudoElement(Element*);
  ~FirstLetterPseudoElement() override;

  static LayoutText* FirstLetterTextLayoutObject(const Element&);
  static unsigned FirstLetterLength(const String&);

  void ClearRemainingTextLayoutObject();
  LayoutTextFragment* RemainingTextLayoutObject() const {
    return remaining_text_layout_object_;
  }

  void UpdateTextFragments();

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(bool performing_reattach) override;
  Node* InnerNodeForHitTesting() const override;

 private:
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject() override;

  void AttachFirstLetterTextLayoutObjects(LayoutText* first_letter_text);

  LayoutTextFragment* remaining_text_layout_object_;
  DISALLOW_COPY_AND_ASSIGN(FirstLetterPseudoElement);
};

template <>
struct DowncastTraits<FirstLetterPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsFirstLetterPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FIRST_LETTER_PSEUDO_ELEMENT_H_
