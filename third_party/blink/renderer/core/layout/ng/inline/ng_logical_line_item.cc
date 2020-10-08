// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"

namespace blink {

const LayoutObject* NGLogicalLineItem::GetLayoutObject() const {
  if (inline_item)
    return inline_item->GetLayoutObject();
  if (const NGPhysicalFragment* fragment = PhysicalFragment())
    return fragment->GetLayoutObject();
  return nullptr;
}

LayoutObject* NGLogicalLineItem::GetMutableLayoutObject() const {
  if (inline_item)
    return inline_item->GetLayoutObject();
  if (const NGPhysicalFragment* fragment = PhysicalFragment())
    return fragment->GetMutableLayoutObject();
  return nullptr;
}

const Node* NGLogicalLineItem::GetNode() const {
  if (const LayoutObject* layout_object = GetLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

const ComputedStyle* NGLogicalLineItem::Style() const {
  if (const auto* fragment = PhysicalFragment())
    return &fragment->Style();
  if (inline_item)
    return inline_item->Style();
  return nullptr;
}

void NGLogicalLineItems::CreateTextFragments(WritingMode writing_mode,
                                             const String& text_content) {
  NGTextFragmentBuilder text_builder(writing_mode);
  for (auto& child : *this) {
    if (const NGInlineItem* inline_item = child.inline_item) {
      if (UNLIKELY(child.text_content)) {
        // Create a generated text fragmment.
        text_builder.SetText(inline_item->GetLayoutObject(), child.text_content,
                             inline_item->Style(), inline_item->StyleVariant(),
                             std::move(child.shape_result), child.MarginSize());
      } else {
        // Create a regular text fragmment.
        DCHECK((inline_item->Type() == NGInlineItem::kText &&
                (inline_item->TextType() == NGTextType::kNormal ||
                 inline_item->TextType() == NGTextType::kSymbolMarker)) ||
               inline_item->Type() == NGInlineItem::kControl);
        text_builder.SetItem(text_content, *inline_item,
                             std::move(child.shape_result), child.text_offset,
                             child.MarginSize());
      }
      text_builder.SetIsHiddenForPaint(child.is_hidden_for_paint);
      DCHECK(!child.text_fragment);
      child.text_fragment = text_builder.ToTextFragment();
    }
  }
}

NGLogicalLineItem* NGLogicalLineItems::FirstInFlowChild() {
  for (auto& child : *this) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

NGLogicalLineItem* NGLogicalLineItems::LastInFlowChild() {
  for (auto it = rbegin(); it != rend(); it++) {
    auto& child = *it;
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

void NGLogicalLineItems::WillInsertChild(unsigned insert_before) {
  unsigned index = 0;
  for (NGLogicalLineItem& child : children_) {
    if (index >= insert_before)
      break;
    if (child.children_count && index + child.children_count > insert_before)
      ++child.children_count;
    ++index;
  }
}

void NGLogicalLineItems::MoveInInlineDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.inline_offset += delta;
}

void NGLogicalLineItems::MoveInInlineDirection(LayoutUnit delta,
                                               unsigned start,
                                               unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.inline_offset += delta;
}

void NGLogicalLineItems::MoveInBlockDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.block_offset += delta;
}

void NGLogicalLineItems::MoveInBlockDirection(LayoutUnit delta,
                                              unsigned start,
                                              unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.block_offset += delta;
}

}  // namespace blink
