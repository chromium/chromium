// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/data_buffer_factory.h"

#include "base/check_op.h"

namespace network {

DataBufferList::Iterator::Iterator() = default;

DataBufferList::Iterator::Iterator(const DataBufferList* data_buffer_list,
                                   uint32_t index)
    : data_buffer_list_(data_buffer_list), index_(index) {}

DataBufferList::Iterator::Iterator(const Iterator& other) = default;

DataBufferList::Iterator::Iterator(Iterator&&) = default;

DataBufferList::Iterator::~Iterator() = default;

DataBufferList::Iterator& DataBufferList::Iterator::operator=(
    const Iterator& other) = default;

DataBufferList::Iterator& DataBufferList::Iterator::operator=(Iterator&&) =
    default;

DataBufferList::Iterator& DataBufferList::Iterator::operator++() {
  if (data_buffer_list_ && index_ < data_buffer_list_->size()) {
    ++index_;
  }
  return *this;
}

DataBufferList::Iterator DataBufferList::Iterator::operator++(int) {
  Iterator temp = *this;
  ++*this;
  return temp;
}

bool DataBufferList::Iterator::operator==(const Iterator& other) const {
  return data_buffer_list_ == other.data_buffer_list_ && index_ == other.index_;
}

bool DataBufferList::Iterator::operator!=(const Iterator& other) const {
  return !(*this == other);
}

base::span<const uint8_t> DataBufferList::Iterator::operator*() const {
  CHECK(data_buffer_list_);
  CHECK_LT(index_, data_buffer_list_->size());
  return data_buffer_list_->Get(index_);
}

DataBufferList::Iterator DataBufferList::begin() const {
  return Iterator(this, 0);
}

DataBufferList::Iterator DataBufferList::end() const {
  return Iterator(this, size());
}

}  // namespace network
