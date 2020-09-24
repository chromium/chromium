/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_FRAGMENT_H_

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class FirstLetterPseudoElement;

// Used to represent a text substring of an element, e.g., for text runs that
// are split because of first letter and that must therefore have different
// styles (and positions in the layout tree).
// We cache offsets so that text transformations can be applied in such a way
// that we can recover the original unaltered string from our corresponding DOM
// node.
class CORE_EXPORT LayoutTextFragment : public LayoutText {
 public:
  ~LayoutTextFragment() override;

  static LayoutTextFragment* Create(Node*,
                                    StringImpl*,
                                    int start_offset,
                                    int length,
                                    LegacyLayout);
  static LayoutTextFragment* CreateAnonymous(PseudoElement&,
                                             StringImpl*,
                                             LegacyLayout);
  static LayoutTextFragment* CreateAnonymous(PseudoElement&,
                                             StringImpl*,
                                             unsigned start,
                                             unsigned length,
                                             LegacyLayout);

  Position PositionForCaretOffset(unsigned) const override;
  base::Optional<unsigned> CaretOffsetForPosition(
      const Position&) const override;

  unsigned Start() const { return start_; }
  unsigned FragmentLength() const { return fragment_length_; }

  unsigned TextStartOffset() const override { return Start(); }

  void SetContentString(StringImpl*);
  StringImpl* ContentString() const { return content_string_.get(); }
  // The complete text is all of the text in the associated DOM text node.
  scoped_refptr<StringImpl> CompleteText() const;
  // The fragment text is the text which will be used by this
  // LayoutTextFragment. For things like first-letter this may differ from the
  // completeText as we maybe using only a portion of the text nodes content.

  scoped_refptr<StringImpl> OriginalText() const override;

  void SetTextFragment(scoped_refptr<StringImpl>,
                       unsigned start,
                       unsigned length);

  void TransformText() override;

  const char* GetName() const override { return "LayoutTextFragment"; }

  void SetFirstLetterPseudoElement(FirstLetterPseudoElement* element) {
    first_letter_pseudo_element_ = element;
  }
  FirstLetterPseudoElement* GetFirstLetterPseudoElement() const {
    return first_letter_pseudo_element_;
  }

  void SetIsRemainingTextLayoutObject(bool is_remaining_text) {
    is_remaining_text_layout_object_ = is_remaining_text;
  }
  bool IsRemainingTextLayoutObject() const {
    return is_remaining_text_layout_object_;
  }

  Text* AssociatedTextNode() const;
  LayoutText* GetFirstLetterPart() const override;

 protected:
  friend class LayoutObjectFactory;
  LayoutTextFragment(Node*, StringImpl*, int start_offset, int length);
  void WillBeDestroyed() override;

 private:
  LayoutBlock* BlockForAccompanyingFirstLetter() const;
  UChar PreviousCharacter() const override;
  void TextDidChange() override;

  void UpdateHitTestResult(HitTestResult&,
                           const PhysicalOffset&) const override;

  unsigned start_;
  unsigned fragment_length_;
  bool is_remaining_text_layout_object_;
  scoped_refptr<StringImpl> content_string_;
  // Reference back to FirstLetterPseudoElement; cleared by
  // FirstLetterPseudoElement::detachLayoutTree() if it goes away first.
  UntracedMember<FirstLetterPseudoElement> first_letter_pseudo_element_;
};

DEFINE_TYPE_CASTS(LayoutTextFragment,
                  LayoutObject,
                  object,
                  (object->IsText() && ToLayoutText(object)->IsTextFragment()),
                  (object.IsText() && ToLayoutText(object).IsTextFragment()));

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TEXT_FRAGMENT_H_
