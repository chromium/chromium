// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"

namespace blink {

const LayoutObject* LogicalLineItem::GetLayoutObject() const {
  if (inline_item)
    return inline_item->GetLayoutObject();
  if (const auto* fragment = GetPhysicalFragment()) {
    return fragment->GetLayoutObject();
  }
  return nullptr;
}

LayoutObject* LogicalLineItem::GetMutableLayoutObject() const {
  if (inline_item)
    return inline_item->GetLayoutObject();
  if (const auto* fragment = GetPhysicalFragment()) {
    return fragment->GetMutableLayoutObject();
  }
  return nullptr;
}

const Node* LogicalLineItem::GetNode() const {
  if (const LayoutObject* object = GetLayoutObject())
    return object->GetNode();
  return nullptr;
}

const ComputedStyle* LogicalLineItem::Style() const {
  if (const auto* fragment = GetPhysicalFragment()) {
    return &fragment->Style();
  }
  if (inline_item)
    return inline_item->Style();
  return nullptr;
}

std::ostream& operator<<(std::ostream& stream, const LogicalLineItem& item) {
  stream << "LogicalLineItem(";
  if (item.IsPlaceholder())
    stream << " placeholder";
  stream << " inline_size=" << item.inline_size;
  if (item.inline_item)
    stream << " " << item.inline_item->ToString().Utf8().c_str();
  if (item.GetPhysicalFragment()) {
    stream << " Fragment=" << *item.GetPhysicalFragment();
  }
  if (item.GetLayoutObject())
    stream << " LayoutObject=" << *item.GetLayoutObject();
  stream << ")";
  // Feel free to add more information.
  return stream;
}

LogicalLineItem* LogicalLineItems::FirstInFlowChild() {
  for (auto& child : *this) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

LogicalLineItem* LogicalLineItems::LastInFlowChild() {
  for (auto& child : base::Reversed(*this)) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

const LayoutResult* LogicalLineItems::BlockInInlineLayoutResult() const {
  for (const LogicalLineItem& item : *this) {
    if (item.layout_result &&
        item.layout_result->GetPhysicalFragment().IsBlockInInline()) {
      return item.layout_result.Get();
    }
  }
  return nullptr;
}

void LogicalLineItems::WillInsertChild(unsigned insert_before) {
  unsigned index = 0;
  for (LogicalLineItem& child : children_) {
    if (index >= insert_before)
      break;
    if (child.children_count && index + child.children_count > insert_before)
      ++child.children_count;
    ++index;
  }
}

void LogicalLineItems::MoveInInlineDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.inline_offset += delta;
}

void LogicalLineItems::MoveInInlineDirection(LayoutUnit delta,
                                             unsigned start,
                                             unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.inline_offset += delta;
}

void LogicalLineItems::MoveInBlockDirection(LayoutUnit delta) {
  for (auto& child : children_)
    child.rect.offset.block_offset += delta;
}

void LogicalLineItems::MoveInBlockDirection(LayoutUnit delta,
                                            unsigned start,
                                            unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].rect.offset.block_offset += delta;
}

void LogicalLineItem::Trace(Visitor* visitor) const {
  visitor->Trace(shape_result);
  visitor->Trace(layout_result);
  visitor->Trace(layout_object);
  visitor->Trace(out_of_flow_positioned_box);
  visitor->Trace(unpositioned_float);
}

void LogicalLineItems::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
}

}  // namespace blink
