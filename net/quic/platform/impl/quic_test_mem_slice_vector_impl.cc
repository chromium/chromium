// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_test_mem_slice_vector_impl.h"

namespace quic {
namespace test {

TestIOBuffer::~TestIOBuffer() {
  data_ = nullptr;
}

QuicTestMemSliceVectorImpl::~QuicTestMemSliceVectorImpl() {}

QuicTestMemSliceVectorImpl::QuicTestMemSliceVectorImpl(
    std::vector<std::pair<char*, size_t>> buffers) {
  for (auto& buffer : buffers) {
    buffers_.push_back(base::MakeRefCounted<TestIOBuffer>(buffer.first));
    lengths_.push_back(buffer.second);
  }
}

QuicTestMemSliceVectorImpl::QuicTestMemSliceVectorImpl(
    QuicTestMemSliceVectorImpl&& other) {
  *this = std::move(other);
}

QuicTestMemSliceVectorImpl& QuicTestMemSliceVectorImpl::operator=(
    QuicTestMemSliceVectorImpl&& other) {
  if (this != &other) {
    buffers_ = std::move(other.buffers_);
    lengths_ = std::move(other.lengths_);
  }
  return *this;
}

QuicMemSliceSpanImpl QuicTestMemSliceVectorImpl::span() {
  return QuicMemSliceSpanImpl(buffers_.data(), lengths_.data(),
                              buffers_.size());
}

}  // namespace test
}  // namespace quic
