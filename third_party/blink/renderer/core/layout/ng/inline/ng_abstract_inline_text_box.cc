// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_abstract_inline_text_box.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGAbstractInlineTextBox::FragmentToNGAbstractInlineTextBoxHashMap*
    NGAbstractInlineTextBox::g_abstract_inline_text_box_map_ = nullptr;

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::GetOrCreate(
    const NGPaintFragment& fragment) {
  DCHECK(fragment.GetLayoutObject()->IsText()) << fragment.GetLayoutObject();
  if (!g_abstract_inline_text_box_map_) {
    g_abstract_inline_text_box_map_ =
        new FragmentToNGAbstractInlineTextBoxHashMap();
  }
  const auto it = g_abstract_inline_text_box_map_->find(&fragment);
  LayoutText* const layout_text =
      ToLayoutText(fragment.GetMutableLayoutObject());
  if (it != g_abstract_inline_text_box_map_->end()) {
    CHECK(layout_text->HasAbstractInlineTextBox());
    return it->value;
  }
  scoped_refptr<AbstractInlineTextBox> obj = base::AdoptRef(
      new NGAbstractInlineTextBox(LineLayoutText(layout_text), fragment));
  g_abstract_inline_text_box_map_->Set(&fragment, obj);
  layout_text->SetHasAbstractInlineTextBox();
  return obj;
}

void NGAbstractInlineTextBox::WillDestroy(NGPaintFragment* fragment) {
  if (!g_abstract_inline_text_box_map_)
    return;
  const auto it = g_abstract_inline_text_box_map_->find(fragment);
  if (it != g_abstract_inline_text_box_map_->end()) {
    it->value->Detach();
    g_abstract_inline_text_box_map_->erase(fragment);
  }
}

NGAbstractInlineTextBox::NGAbstractInlineTextBox(
    LineLayoutText line_layout_item,
    const NGPaintFragment& fragment)
    : AbstractInlineTextBox(line_layout_item), fragment_(&fragment) {
  DCHECK(fragment_->PhysicalFragment().IsText()) << fragment_;
}

NGAbstractInlineTextBox::~NGAbstractInlineTextBox() {
  DCHECK(!fragment_);
}

void NGAbstractInlineTextBox::Detach() {
  if (Node* const node = GetNode()) {
    if (AXObjectCache* cache = node->GetDocument().ExistingAXObjectCache())
      cache->InlineTextBoxesUpdated(GetLineLayoutItem());
  }
  AbstractInlineTextBox::Detach();
  fragment_ = nullptr;
}

const NGPhysicalTextFragment& NGAbstractInlineTextBox::PhysicalTextFragment()
    const {
  return To<NGPhysicalTextFragment>(fragment_->PhysicalFragment());
}

bool NGAbstractInlineTextBox::NeedsLayout() const {
  return fragment_->GetLayoutObject()->NeedsLayout();
}

bool NGAbstractInlineTextBox::NeedsTrailingSpace() const {
  if (!fragment_->Style().CollapseWhiteSpace())
    return false;
  const NGPaintFragment& line_box = *fragment_->ContainerLineBox();
  if (!To<NGPhysicalLineBoxFragment>(line_box.PhysicalFragment())
           .HasSoftWrapToNextLine())
    return false;
  const NGPhysicalTextFragment& text_fragment = PhysicalTextFragment();
  if (text_fragment.EndOffset() >= text_fragment.TextContent().length())
    return false;
  if (text_fragment.TextContent()[text_fragment.EndOffset()] != ' ')
    return false;
  const NGInlineBreakToken& break_token = *To<NGInlineBreakToken>(
      To<NGPhysicalLineBoxFragment>(line_box.PhysicalFragment()).BreakToken());
  // TODO(yosin): We should support OOF fragments between |fragment_| and
  // break token.
  if (break_token.TextOffset() != text_fragment.EndOffset() + 1)
    return false;
  // Check a character in text content after |fragment_| comes from same
  // layout text of |fragment_|.
  const NGOffsetMapping* mapping =
      NGOffsetMapping::GetFor(fragment_->GetLayoutObject());
  // TODO(kojii): There's not much we can do for dirty-tree. crbug.com/946004
  if (!mapping)
    return false;
  const base::span<const NGOffsetMappingUnit> mapping_units =
      mapping->GetMappingUnitsForTextContentOffsetRange(
          text_fragment.EndOffset(), text_fragment.EndOffset() + 1);
  if (mapping_units.begin() == mapping_units.end())
    return false;
  const NGOffsetMappingUnit& mapping_unit = mapping_units.front();
  return mapping_unit.GetLayoutObject() == fragment_->GetLayoutObject();
}

const NGPaintFragment*
NGAbstractInlineTextBox::NextTextFragmentForSameLayoutObject() const {
  const auto fragments =
      NGPaintFragment::InlineFragmentsFor(fragment_->GetLayoutObject());
  const auto it =
      std::find_if(fragments.begin(), fragments.end(),
                   [&](const auto& sibling) { return fragment_ == sibling; });
  DCHECK(it != fragments.end());
  const auto next_it = std::next(it);
  return next_it == fragments.end() ? nullptr : *next_it;
}

scoped_refptr<AbstractInlineTextBox>
NGAbstractInlineTextBox::NextInlineTextBox() const {
  if (!fragment_)
    return nullptr;
  DCHECK(!NeedsLayout());
  const NGPaintFragment* next_fragment = NextTextFragmentForSameLayoutObject();
  if (!next_fragment)
    return nullptr;
  return GetOrCreate(*next_fragment);
}

LayoutRect NGAbstractInlineTextBox::LocalBounds() const {
  if (!fragment_ || !GetLineLayoutItem())
    return LayoutRect();
  return LayoutRect(fragment_->InlineOffsetToContainerBox().ToLayoutPoint(),
                    fragment_->Size().ToLayoutSize());
}

unsigned NGAbstractInlineTextBox::Len() const {
  if (!fragment_)
    return 0;
  if (NeedsTrailingSpace())
    return PhysicalTextFragment().TextLength() + 1;
  return PhysicalTextFragment().TextLength();
}

unsigned NGAbstractInlineTextBox::TextOffsetInContainer(unsigned offset) const {
  if (!fragment_)
    return 0;
  return PhysicalTextFragment().StartOffset() + offset;
}

AbstractInlineTextBox::Direction NGAbstractInlineTextBox::GetDirection() const {
  if (!fragment_ || !GetLineLayoutItem())
    return kLeftToRight;
  const TextDirection text_direction =
      PhysicalTextFragment().ResolvedDirection();
  if (GetLineLayoutItem().Style()->IsHorizontalWritingMode())
    return IsLtr(text_direction) ? kLeftToRight : kRightToLeft;
  return IsLtr(text_direction) ? kTopToBottom : kBottomToTop;
}

void NGAbstractInlineTextBox::CharacterWidths(Vector<float>& widths) const {
  if (!fragment_)
    return;
  if (!PhysicalTextFragment().TextShapeResult()) {
    // When |fragment_| for BR, we don't have shape result.
    // "aom-computed-boolean-properties.html" reaches here.
    widths.resize(Len());
    return;
  }
  // TODO(layout-dev): Add support for IndividualCharacterRanges to
  // ShapeResultView to avoid the copy below.
  auto shape_result =
      PhysicalTextFragment().TextShapeResult()->CreateShapeResult();
  Vector<CharacterRange> ranges;
  shape_result->IndividualCharacterRanges(&ranges);
  widths.ReserveCapacity(ranges.size());
  widths.resize(0);
  for (const auto& range : ranges)
    widths.push_back(range.Width());
  // The shaper can fail to return glyph metrics for all characters (see
  // crbug.com/613915 and crbug.com/615661) so add empty ranges to ensure all
  // characters have an associated range.
  widths.resize(Len());
}

String NGAbstractInlineTextBox::GetText() const {
  if (!fragment_ || !GetLineLayoutItem())
    return String();

  String result = PhysicalTextFragment().Text().ToString();

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
  if (!fragment_)
    return true;
  DCHECK(!NeedsLayout());
  const auto fragments =
      NGPaintFragment::InlineFragmentsFor(fragment_->GetLayoutObject());
  return fragment_ == &fragments.front();
}

bool NGAbstractInlineTextBox::IsLast() const {
  if (!fragment_)
    return true;
  DCHECK(!NeedsLayout());
  const auto fragments =
      NGPaintFragment::InlineFragmentsFor(fragment_->GetLayoutObject());
  return fragment_ == &fragments.back();
}

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::NextOnLine()
    const {
  if (!fragment_)
    return nullptr;
  DCHECK(!NeedsLayout());
  DCHECK(fragment_->ContainerLineBox());
  NGPaintFragmentTraversal cursor(*fragment_->ContainerLineBox(), *fragment_);
  for (cursor.MoveToNext(); !cursor.IsAtEnd(); cursor.MoveToNext()) {
    if (cursor->GetLayoutObject()->IsText())
      return GetOrCreate(*cursor);
  }
  return nullptr;
}

scoped_refptr<AbstractInlineTextBox> NGAbstractInlineTextBox::PreviousOnLine()
    const {
  if (!fragment_)
    return nullptr;
  DCHECK(!NeedsLayout());
  DCHECK(fragment_->ContainerLineBox());
  NGPaintFragmentTraversal cursor(*fragment_->ContainerLineBox(), *fragment_);
  for (cursor.MoveToPrevious(); !cursor.IsAtEnd(); cursor.MoveToPrevious()) {
    if (cursor->GetLayoutObject()->IsText())
      return GetOrCreate(*cursor);
  }
  return nullptr;
}

bool NGAbstractInlineTextBox::IsLineBreak() const {
  if (!fragment_)
    return false;
  DCHECK(!NeedsLayout());
  return PhysicalTextFragment().IsLineBreak();
}

}  // namespace blink
