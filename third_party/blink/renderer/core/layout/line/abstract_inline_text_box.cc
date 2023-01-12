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
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
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
  GetWordBoundariesForText(words, GetText());

// TODO(crbug/1406930): Uncomment the following DCHECK and fix the dozens of
// failing tests.
// #if DCHECK_IS_ON()
//   if (!words.empty()) {
//     // Validate that our word boundary detection algorithm gives the same output
//     // as the one from the Editing layer.
//     const int initial_offset_in_container =
//         static_cast<int>(TextOffsetInFormattingContext(0));

//     // 1. Compare the word offsets to the ones of the Editing algorithm when
//     // moving forward.
//     Position editing_pos(GetNode(), initial_offset_in_container);
//     int editing_offset =
//         editing_pos.OffsetInContainerNode() - initial_offset_in_container;
//     for (WordBoundaries word : words) {
//       DCHECK_EQ(editing_offset, word.start_index)
//           << "[Going forward] Word boundaries are different between "
//              "accessibility and editing in text=\""
//           << GetText() << "\". Failing at editing text offset \""
//           << editing_offset << "\" and AX text offset \"" << word.start_index
//           << "\".";
//       // See comment in `AbstractInlineTextBox::GetWordBoundariesForText` that
//       // justify why we only check for kWordSkipSpaces.
//       editing_pos =
//           NextWordPosition(editing_pos, PlatformWordBehavior::kWordSkipSpaces)
//               .GetPosition();
//       editing_offset =
//           editing_pos.OffsetInContainerNode() - initial_offset_in_container;
//     }
//     // Check for the last word boundary.
//     DCHECK_EQ(editing_offset, words[words.size() - 1].end_index)
//         << "[Going forward] Word boundaries are different between "
//            "accessibility and at the end of the inline text box. Text=\""
//         << GetText() << "\".";

//     // 2. Compare the word offsets to the ones of the Editing algorithm when
//     // moving backwards.
//     //
//     // TODO(accessibility): Uncomment the following code to validate our word
//     // boundaries also match the ones from the Editing layer when moving
//     // backward. This is currently failing because of crbug/1406287.
//     //
//     // const int last_text_offset =
//     //     initial_offset_in_container + GetText().length();
//     // editing_pos = Position(GetNode(), last_text_offset);
//     // editing_offset = editing_pos.OffsetInContainerNode() -
//     // initial_offset_in_container;

//     // // Check for the first word boundary.
//     // DCHECK_EQ(editing_offset, words[words.size() - 1].end_index)
//     //     << "[Going backward] Word boundaries are different between "
//     //        "accessibility and at the end of the inline text box. Text=\""
//     //     << GetText() << "\".";
//     // editing_pos = PreviousWordPosition(editing_pos).GetPosition();
//     // editing_offset = editing_pos.OffsetInContainerNode() -
//     // initial_offset_in_container;

//     // Vector<WordBoundaries> reverse_words(words);
//     // reverse_words.Reverse();
//     // for (WordBoundaries word : reverse_words) {
//     //   DCHECK_EQ(editing_offset, word.start_index)
//     //       << "[Going backward] Word boundaries are different between "
//     //          "accessibility and editing in text=\""
//     //       << GetText() << "\". Failing at editing text offset \""
//     //       << editing_offset << "\" and AX text offset \"" << word.start_index
//     //       << "\".";
//     //   editing_pos = PreviousWordPosition(editing_pos).GetPosition();
//     //   editing_offset = editing_pos.OffsetInContainerNode() -
//     //   initial_offset_in_container;
//     // }
//   }
// #endif
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
    // before, or immediately preceded by a word break as defined by the Editing
    // code (see `IsWordBreak`). We therefore need to filter the boundaries
    // returned by ICU's WordBreakIterator and return a subset of them. For
    // example we should exclude a word boundary that is between two space
    // characters, "Hello | there".
    //
    // IMPORTANT: This algorithm needs to stay in sync with the one used to
    // find the next/previous word boundary in the Editing layer. See
    // `NextWordPositionInternal` in `visible_units_word.cc` for more info.
    //
    // There's one noticeable difference between our implementation and the one
    // in the Editing layer: in the Editing layer, we only skip spaces before
    // word starts when on Windows. However, we skip spaces the accessible word
    // offsets on all platforms because:
    //   1. It doesn't have an impact on the screen reader user (ATs never
    //      announce spaces).
    //   2. The implementation is simpler. Arguably, this is a bad reason, but
    //      the reality is that word offsets computation will sooner or later
    //      move to the browser process where we'll have to reimplement this
    //      algorithm. Another more near-term possibility is that Editing folks
    //      could refactor their word boundary algorithm so that we could simply
    //      reuse it for accessibility. Anyway, we currently do not see a strong
    //      case to justify spending time to match this behavior perfectly.
    if (WTF::unicode::IsPunct(text[offset]) || U16_IS_SURROGATE(text[offset])) {
      // Case 1: A new word should start before and end after a series of
      // punctuation marks, i.e., Consecutive punctuation marks should be
      // accumulated into a single word. For example, "|Hello|+++---|there|".
      // Surrogate pair runs should also be collapsed.
      //
      // At beginning of text, or right after an alphanumeric character or a
      // character that cannot be a word break.
      if (offset == 0 || WTF::unicode::IsAlphanumeric(text[offset - 1]) ||
          !IsWordBreak(text[offset - 1])) {
        if (word_start)
          words.emplace_back(*word_start, offset);
        word_start = offset;
      } else {
        // Skip to the end of the punctuation/surrogate pair run.
        continue;
      }
    } else if (IsWordBreak(text[offset])) {
      // Case 2: A new word should start if `offset` is before an alphanumeric
      // character, an underscore or a hard line break.
      //
      // We found a new word start or end. Append the previous word (if it
      // exists) to the results, otherwise save this offset as a word start.
      if (word_start) {
        words.emplace_back(*word_start, offset);
      }
      word_start = offset;
    } else if (offset > 0) {
      // Case 3: A word should end if `offset` is proceeded by a word break or
      // a punctuation.
      UChar prev_character = text[offset - 1];
      if (IsWordBreak(prev_character) ||
          WTF::unicode::IsPunct(prev_character) ||
          U16_IS_SURROGATE(prev_character)) {
        if (word_start) {
          words.emplace_back(*word_start, offset);
          word_start = absl::nullopt;
        }
      }
    }
  }

  // Case 4: If the character at last `offset` in `text` was a word break, then
  // it would have started a new word. We need to add its corresponding word end
  // boundary which should be at `text`'s length.
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
