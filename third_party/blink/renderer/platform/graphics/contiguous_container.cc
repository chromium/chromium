// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"

#include <algorithm>
#include <memory>

namespace blink {

ContiguousContainerBase::ContiguousContainerBase(
    wtf_size_t max_item_size,
    wtf_size_t initial_capacity_in_bytes)
    : max_item_size_(max_item_size),
      initial_capacity_in_bytes_(
          std::max(max_item_size, initial_capacity_in_bytes)) {}

ContiguousContainerBase::~ContiguousContainerBase() = default;

wtf_size_t ContiguousContainerBase::CapacityInBytes() const {
  wtf_size_t capacity = 0;
  for (const auto& buffer : buffers_)
    capacity += buffer.Capacity();
  return capacity;
}

wtf_size_t ContiguousContainerBase::UsedCapacityInBytes() const {
  wtf_size_t used_capacity = 0;
  for (const auto& buffer : buffers_)
    used_capacity += buffer.UsedCapacity();
  return used_capacity;
}

wtf_size_t ContiguousContainerBase::MemoryUsageInBytes() const {
  return sizeof(*this) + CapacityInBytes() + items_.CapacityInBytes();
}

uint8_t* ContiguousContainerBase::Allocate(wtf_size_t item_size,
                                           const char* type_name) {
  DCHECK_LE(item_size, max_item_size_);

  Buffer* buffer_for_alloc = nullptr;
  if (!buffers_.IsEmpty() && buffers_.back().UnusedCapacity() >= item_size)
    buffer_for_alloc = &buffers_.back();

  if (!buffer_for_alloc) {
    wtf_size_t new_buffer_size = buffers_.IsEmpty()
                                     ? initial_capacity_in_bytes_
                                     : 2 * buffers_.back().Capacity();
    buffer_for_alloc = &buffers_.emplace_back(new_buffer_size, type_name);
  }

  uint8_t* item = buffer_for_alloc->Allocate(item_size);
  items_.push_back(item);
  return item;
}

}  // namespace blink
