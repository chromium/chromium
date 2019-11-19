// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/contiguous_container.h"

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"

namespace blink {

// Default number of max-sized elements to allocate space for, if there is no
// initial buffer.
static const unsigned kDefaultInitialBufferSize = 32;

class ContiguousContainerBase::Buffer {
  USING_FAST_MALLOC(Buffer);

 public:
  Buffer(size_t buffer_size, const char* type_name) {
    capacity_ = WTF::Partitions::BufferActualSize(buffer_size);
    begin_ = end_ =
        static_cast<char*>(WTF::Partitions::BufferMalloc(capacity_, type_name));
    ANNOTATE_NEW_BUFFER(begin_, capacity_, 0);
  }

  ~Buffer() {
    ANNOTATE_DELETE_BUFFER(begin_, capacity_, UsedCapacity());
    WTF::Partitions::BufferFree(begin_);
  }

  size_t Capacity() const { return capacity_; }
  size_t UsedCapacity() const { return end_ - begin_; }
  size_t UnusedCapacity() const { return Capacity() - UsedCapacity(); }
  bool IsEmpty() const { return UsedCapacity() == 0; }

  void* Allocate(size_t object_size) {
    DCHECK_GE(UnusedCapacity(), object_size);
    ANNOTATE_CHANGE_SIZE(begin_, capacity_, UsedCapacity(),
                         UsedCapacity() + object_size);
    void* result = end_;
    end_ += object_size;
    return result;
  }

  void DeallocateLastObject(void* object) {
    CHECK_LE(begin_, object);
    CHECK_LT(object, end_);
    ANNOTATE_CHANGE_SIZE(begin_, capacity_, UsedCapacity(),
                         static_cast<char*>(object) - begin_);
    end_ = static_cast<char*>(object);
  }

 private:
  // m_begin <= m_end <= m_begin + m_capacity
  char* begin_;
  char* end_;
  size_t capacity_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

ContiguousContainerBase::ContiguousContainerBase(size_t max_object_size)
    : end_index_(0), max_object_size_(max_object_size) {}

ContiguousContainerBase::ContiguousContainerBase(
    ContiguousContainerBase&& source)
    : ContiguousContainerBase(source.max_object_size_) {
  Swap(source);
}

ContiguousContainerBase::~ContiguousContainerBase() = default;

ContiguousContainerBase& ContiguousContainerBase::operator=(
    ContiguousContainerBase&& source) {
  Swap(source);
  return *this;
}

size_t ContiguousContainerBase::CapacityInBytes() const {
  size_t capacity = 0;
  for (const auto& buffer : buffers_)
    capacity += buffer->Capacity();
  return capacity;
}

size_t ContiguousContainerBase::UsedCapacityInBytes() const {
  size_t used_capacity = 0;
  for (const auto& buffer : buffers_)
    used_capacity += buffer->UsedCapacity();
  return used_capacity;
}

size_t ContiguousContainerBase::MemoryUsageInBytes() const {
  return sizeof(*this) + CapacityInBytes() +
         elements_.capacity() * sizeof(elements_[0]);
}

void ContiguousContainerBase::ReserveInitialCapacity(size_t buffer_size,
                                                     const char* type_name) {
  AllocateNewBufferForNextAllocation(buffer_size, type_name);
}

void* ContiguousContainerBase::Allocate(size_t object_size,
                                        const char* type_name) {
  DCHECK_LE(object_size, max_object_size_);

  Buffer* buffer_for_alloc = nullptr;
  if (!buffers_.IsEmpty()) {
    Buffer* end_buffer = buffers_[end_index_].get();
    if (end_buffer->UnusedCapacity() >= object_size)
      buffer_for_alloc = end_buffer;
    else if (end_index_ + 1 < buffers_.size())
      buffer_for_alloc = buffers_[++end_index_].get();
  }

  if (!buffer_for_alloc) {
    size_t new_buffer_size = buffers_.IsEmpty()
                                 ? kDefaultInitialBufferSize * max_object_size_
                                 : 2 * buffers_.back()->Capacity();
    buffer_for_alloc =
        AllocateNewBufferForNextAllocation(new_buffer_size, type_name);
  }

  void* element = buffer_for_alloc->Allocate(object_size);
  elements_.push_back(element);
  return element;
}

void ContiguousContainerBase::RemoveLast() {
  void* object = elements_.back();
  elements_.pop_back();

  Buffer* end_buffer = buffers_[end_index_].get();
  end_buffer->DeallocateLastObject(object);

  if (end_buffer->IsEmpty()) {
    if (end_index_ > 0)
      end_index_--;
    if (end_index_ + 2 < buffers_.size())
      buffers_.pop_back();
  }
}

void ContiguousContainerBase::Clear() {
  elements_.clear();
  buffers_.clear();
  end_index_ = 0;
}

void ContiguousContainerBase::Swap(ContiguousContainerBase& other) {
  elements_.swap(other.elements_);
  buffers_.swap(other.buffers_);
  std::swap(end_index_, other.end_index_);
  std::swap(max_object_size_, other.max_object_size_);
}

void ContiguousContainerBase::ShrinkToFit() {
  while (end_index_ < buffers_.size() - 1) {
    DCHECK(buffers_.back()->IsEmpty());
    buffers_.pop_back();
  }
}

ContiguousContainerBase::Buffer*
ContiguousContainerBase::AllocateNewBufferForNextAllocation(
    size_t buffer_size,
    const char* type_name) {
  DCHECK(buffers_.IsEmpty() || end_index_ == buffers_.size() - 1);
  std::unique_ptr<Buffer> new_buffer =
      std::make_unique<Buffer>(buffer_size, type_name);
  Buffer* buffer_to_return = new_buffer.get();
  buffers_.push_back(std::move(new_buffer));
  end_index_ = buffers_.size() - 1;
  return buffer_to_return;
}

}  // namespace blink
