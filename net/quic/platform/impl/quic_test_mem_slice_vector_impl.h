// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_MEM_SLICE_VECTOR_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_MEM_SLICE_VECTOR_IMPL_H_

#include <memory>

#include "net/quic/platform/impl/quic_mem_slice_span_impl.h"

namespace quic {

namespace test {

class TestIOBuffer : public net::IOBuffer {
 public:
  explicit TestIOBuffer(char* data) : net::IOBuffer(data) {}

 private:
  ~TestIOBuffer() override;
};

class QuicTestMemSliceVectorImpl {
 public:
  explicit QuicTestMemSliceVectorImpl(
      std::vector<std::pair<char*, size_t>> buffers);
  ~QuicTestMemSliceVectorImpl();

  QuicTestMemSliceVectorImpl(QuicTestMemSliceVectorImpl&& other);
  QuicTestMemSliceVectorImpl& operator=(QuicTestMemSliceVectorImpl&& other);

  QuicMemSliceSpanImpl span();

 private:
  std::vector<scoped_refptr<net::IOBuffer>> buffers_;
  std::vector<size_t> lengths_;
};

}  // namespace test

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_MEM_SLICE_VECTOR_IMPL_H_
