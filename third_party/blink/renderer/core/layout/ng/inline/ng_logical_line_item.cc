// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"

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
  if (const LayoutObject* object = GetLayoutObject())
    return object->GetNode();
  return nullptr;
}

const ComputedStyle* NGLogicalLineItem::Style() const {
  if (const auto* fragment = PhysicalFragment())
    return &fragment->Style();
  if (inline_item)
    return inline_item->Style();
  return nullptr;
}

std::ostream& operator<<(std::ostream& stream, const NGLogicalLineItem& item) {
  stream << "NGLogicalLineItem(";
  if (item.IsPlaceholder())
    stream << " placeholder";
  stream << " inline_size=" << item.inline_size;
  if (item.inline_item)
    stream << " " << item.inline_item->ToString().Utf8().c_str();
  if (item.PhysicalFragment())
    stream << " Fragment=" << *item.PhysicalFragment();
  if (item.GetLayoutObject())
    stream << " LayoutObject=" << *item.GetLayoutObject();
  stream << ")";
  // Feel free to add more information.
  return stream;
}

NGLogicalLineItem* NGLogicalLineItems::FirstInFlowChild() {
  for (auto& child : *this) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

NGLogicalLineItem* NGLogicalLineItems::LastInFlowChild() {
  for (auto& child : base::Reversed(*this)) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

const NGLayoutResult* NGLogicalLineItems::BlockInInlineLayoutResult() const {
  for (const NGLogicalLineItem& item : *this) {
    if (item.layout_result &&
        item.layout_result->PhysicalFragment().IsBlockInInline())
      return item.layout_result;
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

void NGLogicalLineItem::Trace(Visitor* visitor) const {
  visitor->Trace(layout_result);
  visitor->Trace(layout_object);
  visitor->Trace(out_of_flow_positioned_box);
  visitor->Trace(unpositioned_float);
}

void NGLogicalLineItems::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
}

}  // namespace blink
