/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

typedef HeapHashMap<Member<InlineTextBox>, scoped_refptr<AbstractInlineTextBox>>
    InlineToLegacyAbstractInlineTextBoxHashMap;

InlineToLegacyAbstractInlineTextBoxHashMap& GetAbstractInlineTextBoxMap() {
  DEFINE_STATIC_LOCAL(
      Persistent<InlineToLegacyAbstractInlineTextBoxHashMap>,
      abstract_inline_text_box_map,
      (MakeGarbageCollected<InlineToLegacyAbstractInlineTextBoxHashMap>()));
  return *abstract_inline_text_box_map;
}

AbstractInlineTextBox::AbstractInlineTextBox(LineLayoutText line_layout_item)
    : line_layout_item_(line_layout_item) {}

AbstractInlineTextBox::~AbstractInlineTextBox() {
  DCHECK(!line_layout_item_);
}

LayoutText* AbstractInlineTextBox::GetFirstLetterPseudoLayoutText() const {
  // We only want to apply the first letter to the first inline text box
  // for a LayoutObject.
  if (!IsFirst())
    return nullptr;

  Node* node = GetLineLayoutItem().GetNode();
  if (!node)
    return nullptr;
  if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject()))
    return layout_text->GetFirstLetterPart();
  return nullptr;
}

// ----

scoped_refptr<AbstractInlineTextBox> LegacyAbstractInlineTextBox::GetOrCreate(
    LineLayoutText line_layout_text,
    InlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  InlineToLegacyAbstractInlineTextBoxHashMap::const_iterator it =
      GetAbstractInlineTextBoxMap().find(inline_text_box);
  if (it != GetAbstractInlineTextBoxMap().end())
    return it->value;

  scoped_refptr<AbstractInlineTextBox> obj = base::AdoptRef(
      new LegacyAbstractInlineTextBox(line_layout_text, inline_text_box));
  GetAbstractInlineTextBoxMap().Set(inline_text_box, obj);
  return obj;
}

void LegacyAbstractInlineTextBox::WillDestroy(InlineTextBox* inline_text_box) {
  InlineToLegacyAbstractInlineTextBoxHashMap::const_iterator it =
      GetAbstractInlineTextBoxMap().find(inline_text_box);
  if (it != GetAbstractInlineTextBoxMap().end()) {
    it->value->Detach();
    GetAbstractInlineTextBoxMap().erase(inline_text_box);
  }
}

LegacyAbstractInlineTextBox::LegacyAbstractInlineTextBox(
    LineLayoutText line_layout_item,
    InlineTextBox* inline_text_box)
    : AbstractInlineTextBox(line_layout_item),
      inline_text_box_(inline_text_box) {}

LegacyAbstractInlineTextBox::~LegacyAbstractInlineTextBox() {
  DCHECK(!inline_text_box_);
}

void AbstractInlineTextBox::Detach() {
  DCHECK(GetLineLayoutItem());
  if (AXObjectCache* cache = ExistingAXObjectCache())
    cache->Remove(this);

  line_layout_item_ = LineLayoutText(nullptr);
}

void LegacyAbstractInlineTextBox::Detach() {
  AbstractInlineTextBox::Detach();
  inline_text_box_ = nullptr;
}

scoped_refptr<AbstractInlineTextBox>
LegacyAbstractInlineTextBox::NextInlineTextBox() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return nullptr;

  return GetOrCreate(GetLineLayoutItem(),
                     inline_text_box_->NextForSameLayoutObject());
}

LayoutRect LegacyAbstractInlineTextBox::LocalBounds() const {
  if (!inline_text_box_ || !GetLineLayoutItem())
    return LayoutRect();

  return inline_text_box_->FrameRect();
}

unsigned LegacyAbstractInlineTextBox::Len() const {
  if (!inline_text_box_)
    return 0u;

  return NeedsTrailingSpace() ? inline_text_box_->Len() + 1
                              : inline_text_box_->Len();
}

unsigned LegacyAbstractInlineTextBox::TextOffsetInFormattingContext(
    unsigned offset) const {
  if (!inline_text_box_)
    return 0U;

  // The start offset of the inline text box returned by
  // inline_text_box_->Start() includes the collapsed white-spaces in the inline
  // box's parent, which could be e.g. a text node or a br element. Here, we
  // want the position in the layout block flow ancestor object after
  // white-space collapsing.
  //
  // NGOffsetMapping can map an offset before whites-spaces are collapsed to the
  // offset after white-spaces are collapsed even when using Legacy Layout.
  unsigned int offset_in_parent = inline_text_box_->Start() + offset;

  const Node* node = GetNode();
  // If the associated node is a text node, then |offset_in_parent| is a text
  // offset, otherwise we can't represent the exact offset using a DOM position.
  // We fall back to using the layout object associated with this inline text
  // box. In other words, if the associated node is a text node, then we can
  // return a more exact offset in our formatting context. Otherwise, we need to
  // approximate the offset using our associated layout object.
  if (node && node->IsTextNode()) {
    const Position position(node, static_cast<int>(offset_in_parent));
    LayoutBlockFlow* formatting_context =
        NGOffsetMapping::GetInlineFormattingContextOf(position);
    // If "formatting_context" is not a Layout NG object, the offset mappings
    // will be computed on demand and cached.
    const NGOffsetMapping* offset_mapping =
        formatting_context ? NGInlineNode::GetOffsetMapping(formatting_context)
                           : nullptr;
    if (!offset_mapping)
      return offset_in_parent;

    return offset_mapping->GetTextContentOffset(position).value_or(
        offset_in_parent);
  }

  const LayoutObject* layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(GetLineLayoutItem());
  DCHECK(layout_object);
  LayoutBlockFlow* formatting_context =
      NGOffsetMapping::GetInlineFormattingContextOf(*layout_object);
  const NGOffsetMapping* offset_mapping =
      formatting_context ? NGInlineNode::GetOffsetMapping(formatting_context)
                         : nullptr;
  if (!offset_mapping)
    return offset_in_parent;

  base::span<const NGOffsetMappingUnit> mapping_units =
      offset_mapping->GetMappingUnitsForLayoutObject(*layout_object);
  if (mapping_units.empty())
    return offset_in_parent;
  return mapping_units.front().ConvertDOMOffsetToTextContent(offset_in_parent);
}

AbstractInlineTextBox::Direction LegacyAbstractInlineTextBox::GetDirection()
    const {
  if (!inline_text_box_ || !GetLineLayoutItem())
    return kLeftToRight;

  if (GetLineLayoutItem().StyleRef().IsHorizontalWritingMode()) {
    return (inline_text_box_->Direction() == TextDirection::kRtl
                ? kRightToLeft
                : kLeftToRight);
  }
  return (inline_text_box_->Direction() == TextDirection::kRtl ? kBottomToTop
                                                               : kTopToBottom);
}

Node* AbstractInlineTextBox::GetNode() const {
  if (!GetLineLayoutItem())
    return nullptr;
  return GetLineLayoutItem().GetNode();
}

LayoutObject* AbstractInlineTextBox::GetLayoutObject() const {
  if (!GetLineLayoutItem())
    return nullptr;
  return GetLineLayoutItem().GetLayoutObject();
}

AXObjectCache* AbstractInlineTextBox::ExistingAXObjectCache() const {
  if (LayoutObject* layout_object = GetLayoutObject())
    return layout_object->GetDocument().ExistingAXObjectCache();
  return nullptr;
}

void LegacyAbstractInlineTextBox::CharacterWidths(Vector<float>& widths) const {
  if (!inline_text_box_)
    return;

  inline_text_box_->CharacterWidths(widths);
  if (NeedsTrailingSpace())
    widths.push_back(inline_text_box_->NewlineSpaceWidth());
}

void AbstractInlineTextBox::GetWordBoundaries(
    Vector<WordBoundaries>& words) const {
  return GetWordBoundariesForText(words, GetText());
}

// static
void AbstractInlineTextBox::GetWordBoundariesForText(
    Vector<WordBoundaries>& words,
    const String& text) {
  if (!text.length())
    return;

  TextBreakIterator* it = WordBreakIterator(text, 0, text.length());
  if (!it)
    return;
  absl::optional<int> word_start;
  for (int offset = 0;
       offset != kTextBreakDone && offset < static_cast<int>(text.length());
       offset = it->following(offset)) {
    // Unlike in ICU's WordBreakIterator, a word boundary is valid only if it is
    // before, or immediately preceded by, an alphanumeric character, a series
    // of punctuation marks, an underscore or a line break. We therefore need to
    // filter the boundaries returned by ICU's WordBreakIterator and return a
    // subset of them. For example we should exclude a word boundary that is
    // between two space characters, "Hello | there".

    // Case 1: A new word should start if |offset| is before an alphanumeric
    // character, an underscore or a hard line break
    if (WTF::unicode::IsAlphanumeric(text[offset]) ||
        text[offset] == kLowLineCharacter ||
        text[offset] == kNewlineCharacter ||
        text[offset] == kCarriageReturnCharacter) {
      // We found a new word start or end. Append the previous word (if it
      // exists) to the results, otherwise save this offset as a word start.
      if (word_start)
        words.emplace_back(*word_start, offset);
      word_start = offset;

      // Case 2: A new word should start before and end after a series of
      // punctuation marks, i.e., Consecutive punctuation marks should be
      // accumulated into a single word. For example, "|Hello|+++---|there|".
    } else if (WTF::unicode::IsPunct(text[offset])) {
      // At beginning of text, or the previous character was a punctuation
      // symbol.
      if (offset == 0 || !WTF::unicode::IsPunct(text[offset - 1])) {
        if (word_start)
          words.emplace_back(*word_start, offset);
        word_start = offset;
      }
      continue;  // Skip to the end of the punctuation run.

      // Case 3: A word should end if |offset| is proceeded by an alphanumeric
      // character, a series of punctuation marks, an underscore or a hard line
      // break.
    } else if (offset > 0) {
      UChar prev_character = text[offset - 1];
      if (WTF::unicode::IsAlphanumeric(prev_character) ||
          WTF::unicode::IsPunct(prev_character) ||
          prev_character == kLowLineCharacter ||
          prev_character == kNewlineCharacter ||
          prev_character == kCarriageReturnCharacter) {
        if (word_start) {
          words.emplace_back(*word_start, offset);
          word_start = absl::nullopt;
        }
      }
    }
  }

  // Case 4: If the character at last |offset| in |text| was an alphanumeric
  // character, a punctuation mark, an underscore, or a line break, then it
  // would have started a new word. We need to add its corresponding word end
  // boundary which should be at |text|'s length.
  if (word_start) {
    words.emplace_back(*word_start, text.length());
    word_start = absl::nullopt;
  }
}

String LegacyAbstractInlineTextBox::GetText() const {
  if (!inline_text_box_ || !GetLineLayoutItem())
    return String();

  String result = inline_text_box_->GetText();

  // Change all whitespace to just a space character, except for
  // actual line breaks.
  if (!inline_text_box_->IsLineBreak())
    result = result.SimplifyWhiteSpace(WTF::kDoNotStripWhiteSpace);

  // When the CSS first-letter pseudoselector is used, the LayoutText for the
  // first letter is excluded from the accessibility tree, so we need to prepend
  // its text here.
  if (LayoutText* first_letter = GetFirstLetterPseudoLayoutText()) {
    result = first_letter->GetText().SimplifyWhiteSpace() + result;
  }

  // Insert a space at the end of this if necessary.
  if (NeedsTrailingSpace())
    return result + " ";

  return result;
}

bool LegacyAbstractInlineTextBox::IsFirst() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  return !inline_text_box_ || !inline_text_box_->PrevForSameLayoutObject();
}

bool LegacyAbstractInlineTextBox::IsLast() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  return !inline_text_box_ || !inline_text_box_->NextForSameLayoutObject();
}

scoped_refptr<AbstractInlineTextBox> LegacyAbstractInlineTextBox::NextOnLine()
    const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return nullptr;

  InlineBox* next = inline_text_box_->NextOnLine();
  if (auto* text_box = DynamicTo<InlineTextBox>(next))
    return GetOrCreate(text_box->GetLineLayoutItem(), text_box);

  return nullptr;
}

scoped_refptr<AbstractInlineTextBox>
LegacyAbstractInlineTextBox::PreviousOnLine() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return nullptr;

  InlineBox* previous = inline_text_box_->PrevOnLine();
  if (auto* text_box = DynamicTo<InlineTextBox>(previous))
    return GetOrCreate(text_box->GetLineLayoutItem(), text_box);

  return nullptr;
}

bool LegacyAbstractInlineTextBox::IsLineBreak() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return false;

  return inline_text_box_->IsLineBreak();
}

bool LegacyAbstractInlineTextBox::NeedsTrailingSpace() const {
  if (const InlineTextBox* next = inline_text_box_->NextForSameLayoutObject()) {
    return next->Start() >
               inline_text_box_->Start() + inline_text_box_->Len() &&
           inline_text_box_->GetText().length() &&
           !inline_text_box_->GetText()
                .Right(1)
                .ContainsOnlyWhitespaceOrEmpty() &&
           next->GetText().length() &&
           !next->GetText().Left(1).ContainsOnlyWhitespaceOrEmpty();
  }
  return false;
}

}  // namespace blink
