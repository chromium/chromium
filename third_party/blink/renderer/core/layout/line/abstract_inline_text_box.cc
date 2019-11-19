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
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

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

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || !layout_object->IsText())
    return nullptr;
  return ToLayoutText(layout_object)->GetFirstLetterPart();
}

// ----

LegacyAbstractInlineTextBox::InlineToLegacyAbstractInlineTextBoxHashMap*
    LegacyAbstractInlineTextBox::g_abstract_inline_text_box_map_ = nullptr;

scoped_refptr<AbstractInlineTextBox> LegacyAbstractInlineTextBox::GetOrCreate(
    LineLayoutText line_layout_text,
    InlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  if (!g_abstract_inline_text_box_map_) {
    g_abstract_inline_text_box_map_ =
        new InlineToLegacyAbstractInlineTextBoxHashMap();
  }

  InlineToLegacyAbstractInlineTextBoxHashMap::const_iterator it =
      g_abstract_inline_text_box_map_->find(inline_text_box);
  if (it != g_abstract_inline_text_box_map_->end())
    return it->value;

  scoped_refptr<AbstractInlineTextBox> obj = base::AdoptRef(
      new LegacyAbstractInlineTextBox(line_layout_text, inline_text_box));
  g_abstract_inline_text_box_map_->Set(inline_text_box, obj);
  return obj;
}

void LegacyAbstractInlineTextBox::WillDestroy(InlineTextBox* inline_text_box) {
  if (!g_abstract_inline_text_box_map_)
    return;

  InlineToLegacyAbstractInlineTextBoxHashMap::const_iterator it =
      g_abstract_inline_text_box_map_->find(inline_text_box);
  if (it != g_abstract_inline_text_box_map_->end()) {
    it->value->Detach();
    g_abstract_inline_text_box_map_->erase(inline_text_box);
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
  if (Node* node = GetNode()) {
    if (AXObjectCache* cache = node->GetDocument().ExistingAXObjectCache())
      cache->Remove(this);
  }

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
    return 0;

  return inline_text_box_->Len();
}

unsigned LegacyAbstractInlineTextBox::TextOffsetInContainer(
    unsigned offset) const {
  if (!inline_text_box_)
    return 0;

  unsigned offset_in_container = inline_text_box_->Start() + offset;

  const NGOffsetMapping* offset_mapping = GetOffsetMapping();
  if (!offset_mapping)
    return offset_in_container;

  // The start offset of the inline text box returned by
  // inline_text_box_->Start() includes the collapsed white-spaces. Here, we
  // want the position in the parent node after white-space collapsing.
  // NGOffsetMapping can map an offset before whites-spaces are collapsed to the
  // offset after white-spaces are collapsed.
  Position position(GetNode(), offset_in_container);
  const NGOffsetMappingUnit* unit =
      offset_mapping->GetMappingUnitForPosition(position);
  return offset_in_container - unit->DOMStart() + unit->TextContentStart();
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

void LegacyAbstractInlineTextBox::CharacterWidths(Vector<float>& widths) const {
  if (!inline_text_box_)
    return;

  inline_text_box_->CharacterWidths(widths);
}

void AbstractInlineTextBox::GetWordBoundaries(
    Vector<WordBoundaries>& words) const {
  if (Len() == 0)
    return;

  String text = GetText();
  int len = text.length();
  TextBreakIterator* iterator = WordBreakIterator(text, 0, len);

  // FIXME: When http://crbug.com/411764 is fixed, replace this with an ASSERT.
  if (!iterator)
    return;

  int pos = iterator->first();
  while (pos >= 0 && pos < len) {
    int next = iterator->next();
    if (IsWordTextBreak(iterator))
      words.push_back(WordBoundaries(pos, next));
    pos = next;
  }
}

String LegacyAbstractInlineTextBox::GetText() const {
  if (!inline_text_box_ || !GetLineLayoutItem())
    return String();

  String result = inline_text_box_->GetText();

  // Simplify all whitespace to just a space character, except for
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
  if (InlineTextBox* next = inline_text_box_->NextForSameLayoutObject()) {
    if (next->Start() > inline_text_box_->Start() + inline_text_box_->Len() &&
        result.length() && !result.Right(1).ContainsOnlyWhitespaceOrEmpty() &&
        next->GetText().length() &&
        !next->GetText().Left(1).ContainsOnlyWhitespaceOrEmpty())
      return result + " ";
  }

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
  if (next && next->IsInlineTextBox())
    return GetOrCreate(ToInlineTextBox(next)->GetLineLayoutItem(),
                       ToInlineTextBox(next));

  return nullptr;
}

scoped_refptr<AbstractInlineTextBox>
LegacyAbstractInlineTextBox::PreviousOnLine() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return nullptr;

  InlineBox* previous = inline_text_box_->PrevOnLine();
  if (previous && previous->IsInlineTextBox())
    return GetOrCreate(ToInlineTextBox(previous)->GetLineLayoutItem(),
                       ToInlineTextBox(previous));

  return nullptr;
}

bool LegacyAbstractInlineTextBox::IsLineBreak() const {
  DCHECK(!inline_text_box_ ||
         !inline_text_box_->GetLineLayoutItem().NeedsLayout());
  if (!inline_text_box_)
    return false;

  return inline_text_box_->IsLineBreak();
}

const NGOffsetMapping* LegacyAbstractInlineTextBox::GetOffsetMapping() const {
  const auto* text_node = DynamicTo<Text>(GetNode());

  LayoutBlockFlow& block_flow = *NGOffsetMapping::GetInlineFormattingContextOf(
      *text_node->GetLayoutObject());
  const NGOffsetMapping* offset_mapping =
      NGInlineNode::GetOffsetMapping(&block_flow);

  if (UNLIKELY(!offset_mapping)) {
    // TODO(crbug.com/955678): There are certain cases where we fail to
    // compute // |NGOffsetMapping| due to failures in layout. As the root
    // cause is hard to fix at the moment, we work around it here so that the
    // production build doesn't crash.
    NOTREACHED();
    return nullptr;
  }
  return offset_mapping;
}

}  // namespace blink
