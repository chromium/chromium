// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_abstract_inline_text_box.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

namespace {

wtf_size_t ItemIndex(const NGInlineCursor& cursor) {
  return static_cast<wtf_size_t>(cursor.CurrentItem() -
                                 &cursor.Items().front());
}

class NGAbstractInlineTextBoxCache final {
 public:
  static NGAbstractInlineTextBox* GetOrCreate(const NGInlineCursor& cursor) {
    if (!s_instance_)
      s_instance_ = new NGAbstractInlineTextBoxCache();
    return s_instance_->GetOrCreateInternal(cursor);
  }

  static void WillDestroy(const NGInlineCursor& cursor) {
    if (!s_instance_)
      return;
    s_instance_->WillDestroyInternal(cursor);
  }

 private:
  NGAbstractInlineTextBoxCache() : map_(MakeGarbageCollected<MapType>()) {}

  NGAbstractInlineTextBox* GetOrCreateInternal(const NGInlineCursor& cursor) {
    DCHECK(cursor.CurrentItem());
    MapKey key = ToMapKey(cursor);
    const auto it = map_->find(key);
    auto* const layout_text =
        To<LayoutText>(cursor.CurrentMutableLayoutObject());
    if (it != map_->end()) {
      CHECK(layout_text->HasAbstractInlineTextBox());
      return it->value;
    }
    auto* obj = MakeGarbageCollected<NGAbstractInlineTextBox>(cursor);
    map_->Set(key, obj);
    layout_text->SetHasAbstractInlineTextBox();
    return obj;
  }

  void WillDestroyInternal(const NGInlineCursor& cursor) {
    MapKey key = ToMapKey(cursor);
    const auto it = map_->find(key);
    if (it == map_->end()) {
      return;
    }
    it->value->Detach();
    map_->erase(key);
  }

  // An NGFragmentItem pointer can't be a key because NGFragmentItem instances
  // are stored in HeapVector instances, and Oilpan heap compaction changes
  // addresses of NGFragmentItem instances.
  using MapKey = std::pair<const NGFragmentItems*, wtf_size_t>;
  MapKey ToMapKey(const NGInlineCursor& cursor) {
    return MapKey(&cursor.Items(), ItemIndex(cursor));
  }

  static NGAbstractInlineTextBoxCache* s_instance_;

  using MapType = HeapHashMap<MapKey, Member<NGAbstractInlineTextBox>>;
  Persistent<MapType> map_;
};

NGAbstractInlineTextBoxCache* NGAbstractInlineTextBoxCache::s_instance_ =
    nullptr;

}  // namespace

NGAbstractInlineTextBox* NGAbstractInlineTextBox::GetOrCreate(
    const NGInlineCursor& cursor) {
  if (!cursor)
    return nullptr;
  return NGAbstractInlineTextBoxCache::GetOrCreate(cursor);
}

void NGAbstractInlineTextBox::WillDestroy(const NGInlineCursor& cursor) {
  if (cursor.CurrentItem()) {
    return NGAbstractInlineTextBoxCache::WillDestroy(cursor);
  }
  NOTREACHED();
}

NGAbstractInlineTextBox::NGAbstractInlineTextBox(const NGInlineCursor& cursor)
    : fragment_item_index_(ItemIndex(cursor)),
      layout_text_(To<LayoutText>(cursor.Current().GetMutableLayoutObject())),
      root_box_fragment_(&cursor.ContainerFragment()) {
  DCHECK(cursor.CurrentItem()->IsText()) << cursor.CurrentItem();
}

NGAbstractInlineTextBox::~NGAbstractInlineTextBox() {
  DCHECK(!fragment_item_index_);
  DCHECK(!root_box_fragment_);
  DCHECK(!layout_text_);
}

void NGAbstractInlineTextBox::Trace(Visitor* visitor) const {
  visitor->Trace(layout_text_);
  visitor->Trace(root_box_fragment_);
}

void NGAbstractInlineTextBox::Detach() {
  LayoutObject* prev_layout_object = GetLayoutText();
  AXObjectCache* cache = ExistingAXObjectCache();

  DCHECK(layout_text_);
  if (cache) {
    cache->Remove(this);
  }

  layout_text_ = nullptr;

  fragment_item_index_ = absl::nullopt;
  root_box_fragment_ = nullptr;

  if (cache) {
    prev_layout_object->CheckIsNotDestroyed();
    DCHECK(IsA<LayoutText>(prev_layout_object));
    cache->InlineTextBoxesUpdated(prev_layout_object);
  }
}

LayoutText* NGAbstractInlineTextBox::GetFirstLetterPseudoLayoutText() const {
  // We only want to apply the first letter to the first inline text box
  // for a LayoutObject.
  if (!IsFirst()) {
    return nullptr;
  }

  Node* node = layout_text_->GetNode();
  if (!node) {
    return nullptr;
  }
  if (auto* layout_text = DynamicTo<LayoutText>(node->GetLayoutObject())) {
    return layout_text->GetFirstLetterPart();
  }
  return nullptr;
}

NGInlineCursor NGAbstractInlineTextBox::GetCursor() const {
  if (!fragment_item_index_) {
    return NGInlineCursor();
  }
  NGInlineCursor cursor(*root_box_fragment_);
  cursor.MoveTo(cursor.Items().Items()[*fragment_item_index_]);
  DCHECK(!cursor.Current().GetLayoutObject()->NeedsLayout());
  return cursor;
}

NGInlineCursor NGAbstractInlineTextBox::GetCursorOnLine() const {
  NGInlineCursor current = GetCursor();
  NGInlineCursor line_box = current;
  line_box.MoveToContainingLine();
  NGInlineCursor cursor = line_box.CursorForDescendants();
  cursor.MoveTo(current);
  return cursor;
}

String NGAbstractInlineTextBox::GetTextContent() const {
  const NGInlineCursor& cursor = GetCursor();
  if (cursor.Current().IsLayoutGeneratedText())
    return cursor.Current().Text(cursor).ToString();
  return cursor.Items().Text(cursor.Current().UsesFirstLineStyle());
}

bool NGAbstractInlineTextBox::NeedsTrailingSpace() const {
  const NGInlineCursor& cursor = GetCursor();
  if (cursor.Current().Style().ShouldPreserveWhiteSpaces()) {
    return false;
  }
  NGInlineCursor line_box = cursor;
  line_box.MoveToContainingLine();
  if (!line_box.Current().HasSoftWrapToNextLine())
    return false;
  const String text_content = GetTextContent();
  const unsigned end_offset = cursor.Current().TextEndOffset();
  if (end_offset >= text_content.length())
    return false;
  if (text_content[end_offset] != ' ')
    return false;
  const NGInlineBreakToken* break_token = line_box.Current().InlineBreakToken();
  // TODO(yosin): We should support OOF fragments between |fragment_| and
  // break token.
  if (break_token && break_token->StartTextOffset() != end_offset + 1) {
    return false;
  }
  // Check a character in text content after |fragment_| comes from same
  // layout text of |fragment_|.
  const LayoutObject* const layout_object = cursor.Current().GetLayoutObject();
  const NGOffsetMapping* mapping = NGOffsetMapping::GetFor(layout_object);
  // TODO(kojii): There's not much we can do for dirty-tree. crbug.com/946004
  if (!mapping)
    return false;
  const base::span<const NGOffsetMappingUnit> mapping_units =
      mapping->GetMappingUnitsForTextContentOffsetRange(end_offset,
                                                        end_offset + 1);
  if (mapping_units.begin() == mapping_units.end())
    return false;
  const NGOffsetMappingUnit& mapping_unit = mapping_units.front();
  return mapping_unit.GetLayoutObject() == layout_object;
}

NGAbstractInlineTextBox* NGAbstractInlineTextBox::NextInlineTextBox() const {
  NGInlineCursor next = GetCursor();
  if (!next)
    return nullptr;
  next.MoveToNextForSameLayoutObject();
  if (!next)
    return nullptr;
  return GetOrCreate(next);
}

LayoutRect NGAbstractInlineTextBox::LocalBounds() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return LayoutRect();
  return cursor.Current().RectInContainerFragment().ToLayoutRect();
}

unsigned NGAbstractInlineTextBox::Len() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return 0;
  if (NeedsTrailingSpace())
    return cursor.Current().Text(cursor).length() + 1;
  return cursor.Current().Text(cursor).length();
}

unsigned NGAbstractInlineTextBox::TextOffsetInFormattingContext(
    unsigned offset) const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return 0;
  return cursor.Current().TextStartOffset() + offset;
}

NGAbstractInlineTextBox::Direction NGAbstractInlineTextBox::GetDirection()
    const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return kLeftToRight;
  const TextDirection text_direction = cursor.Current().ResolvedDirection();
  if (GetLayoutText()->Style()->IsHorizontalWritingMode()) {
    return IsLtr(text_direction) ? kLeftToRight : kRightToLeft;
  }
  return IsLtr(text_direction) ? kTopToBottom : kBottomToTop;
}

Node* NGAbstractInlineTextBox::GetNode() const {
  return layout_text_ ? layout_text_->GetNode() : nullptr;
}

AXObjectCache* NGAbstractInlineTextBox::ExistingAXObjectCache() const {
  return layout_text_ ? layout_text_->GetDocument().ExistingAXObjectCache()
                      : nullptr;
}

void NGAbstractInlineTextBox::CharacterWidths(Vector<float>& widths) const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return;
  const ShapeResultView* shape_result_view = cursor.Current().TextShapeResult();
  if (!shape_result_view) {
    // When |fragment_| for BR, we don't have shape result.
    // "aom-computed-boolean-properties.html" reaches here.
    widths.resize(Len());
    return;
  }
  // TODO(layout-dev): Add support for IndividualCharacterRanges to
  // ShapeResultView to avoid the copy below.
  scoped_refptr<ShapeResult> shape_result =
      shape_result_view->CreateShapeResult();
  Vector<CharacterRange> ranges;
  shape_result->IndividualCharacterRanges(&ranges);
  widths.reserve(ranges.size());
  widths.resize(0);
  for (const auto& range : ranges)
    widths.push_back(range.Width());
  // The shaper can fail to return glyph metrics for all characters (see
  // crbug.com/613915 and crbug.com/615661) so add empty ranges to ensure all
  // characters have an associated range.
  widths.resize(Len());
}

void NGAbstractInlineTextBox::GetWordBoundaries(
    Vector<WordBoundaries>& words) const {
  GetWordBoundariesForText(words, GetText());

  // TODO(crbug/1406930): Uncomment the following DCHECK and fix the dozens of
  // failing tests.
  // #if DCHECK_IS_ON()
  //   if (!words.empty()) {
  //     // Validate that our word boundary detection algorithm gives the same
  //     output
  //     // as the one from the Editing layer.
  //     const int initial_offset_in_container =
  //         static_cast<int>(TextOffsetInFormattingContext(0));

  //     // 1. Compare the word offsets to the ones of the Editing algorithm
  //     when
  //     // moving forward.
  //     Position editing_pos(GetNode(), initial_offset_in_container);
  //     int editing_offset =
  //         editing_pos.OffsetInContainerNode() - initial_offset_in_container;
  //     for (WordBoundaries word : words) {
  //       DCHECK_EQ(editing_offset, word.start_index)
  //           << "[Going forward] Word boundaries are different between "
  //              "accessibility and editing in text=\""
  //           << GetText() << "\". Failing at editing text offset \""
  //           << editing_offset << "\" and AX text offset \"" <<
  //           word.start_index
  //           << "\".";
  //       // See comment in `AbstractInlineTextBox::GetWordBoundariesForText`
  //       that
  //       // justify why we only check for kWordSkipSpaces.
  //       editing_pos =
  //           NextWordPosition(editing_pos,
  //           PlatformWordBehavior::kWordSkipSpaces)
  //               .GetPosition();
  //       editing_offset =
  //           editing_pos.OffsetInContainerNode() -
  //           initial_offset_in_container;
  //     }
  //     // Check for the last word boundary.
  //     DCHECK_EQ(editing_offset, words[words.size() - 1].end_index)
  //         << "[Going forward] Word boundaries are different between "
  //            "accessibility and at the end of the inline text box. Text=\""
  //         << GetText() << "\".";

  //     // 2. Compare the word offsets to the ones of the Editing algorithm
  //     when
  //     // moving backwards.
  //     //
  //     // TODO(accessibility): Uncomment the following code to validate our
  //     word
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
  //     //        "accessibility and at the end of the inline text box.
  //     Text=\""
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
  //     //       << editing_offset << "\" and AX text offset \"" <<
  //     word.start_index
  //     //       << "\".";
  //     //   editing_pos = PreviousWordPosition(editing_pos).GetPosition();
  //     //   editing_offset = editing_pos.OffsetInContainerNode() -
  //     //   initial_offset_in_container;
  //     // }
  //   }
  // #endif
}

// static
void NGAbstractInlineTextBox::GetWordBoundariesForText(
    Vector<WordBoundaries>& words,
    const String& text) {
  if (!text.length()) {
    return;
  }

  TextBreakIterator* it = WordBreakIterator(text, 0, text.length());
  if (!it) {
    return;
  }
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
        if (word_start) {
          words.emplace_back(*word_start, offset);
        }
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

String NGAbstractInlineTextBox::GetText() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return g_empty_string;

  String result = cursor.Current().Text(cursor).ToString();

  // For compatibility with |InlineTextBox|, we should have a space character
  // for soft line break.
  // Following tests require this:
  //  - accessibility/inline-text-change-style.html
  //  - accessibility/inline-text-changes.html
  //  - accessibility/inline-text-word-boundaries.html
  if (NeedsTrailingSpace())
    result = result + " ";

  // When the CSS first-letter pseudoselector is used, the LayoutText for the
  // first letter is excluded from the accessibility tree, so we need to prepend
  // its text here.
  if (LayoutText* first_letter = GetFirstLetterPseudoLayoutText())
    result = first_letter->GetText().SimplifyWhiteSpace() + result;

  return result;
}

bool NGAbstractInlineTextBox::IsFirst() const {
  const NGInlineCursor& cursor = GetCursor();
  if (!cursor)
    return true;
  NGInlineCursor first_fragment;
  first_fragment.MoveTo(*cursor.Current().GetLayoutObject());
  return cursor == first_fragment;
}

bool NGAbstractInlineTextBox::IsLast() const {
  NGInlineCursor cursor = GetCursor();
  if (!cursor)
    return true;
  cursor.MoveToNextForSameLayoutObject();
  return !cursor;
}

NGAbstractInlineTextBox* NGAbstractInlineTextBox::NextOnLine() const {
  NGInlineCursor cursor = GetCursorOnLine();
  if (!cursor)
    return nullptr;
  for (cursor.MoveToNext(); cursor; cursor.MoveToNext()) {
    if (cursor.Current().GetLayoutObject()->IsText())
      return GetOrCreate(cursor);
  }
  return nullptr;
}

NGAbstractInlineTextBox* NGAbstractInlineTextBox::PreviousOnLine() const {
  NGInlineCursor cursor = GetCursorOnLine();
  if (!cursor)
    return nullptr;
  for (cursor.MoveToPrevious(); cursor; cursor.MoveToPrevious()) {
    if (cursor.Current().GetLayoutObject()->IsText())
      return GetOrCreate(cursor);
  }
  return nullptr;
}

bool NGAbstractInlineTextBox::IsLineBreak() const {
  const NGInlineCursor& cursor = GetCursor();
  return cursor && cursor.Current().IsLineBreak();
}

}  // namespace blink
