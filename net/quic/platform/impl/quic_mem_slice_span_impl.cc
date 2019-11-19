// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_mem_slice_span_impl.h"

namespace quic {

QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(
    const scoped_refptr<net::IOBuffer>* buffers,
    const size_t* lengths,
    size_t num_buffers)
    : buffers_(buffers), lengths_(lengths), num_buffers_(num_buffers) {}

QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(QuicMemSliceImpl* slice)
    : QuicMemSliceSpanImpl(slice->impl(), slice->impl_length(), 1) {}

QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(const QuicMemSliceSpanImpl& other) =
    default;
QuicMemSliceSpanImpl& QuicMemSliceSpanImpl::operator=(
    const QuicMemSliceSpanImpl& other) = default;
QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(QuicMemSliceSpanImpl&& other) =
    default;
QuicMemSliceSpanImpl& QuicMemSliceSpanImpl::operator=(
    QuicMemSliceSpanImpl&& other) = default;

QuicMemSliceSpanImpl::~QuicMemSliceSpanImpl() = default;

QuicByteCount QuicMemSliceSpanImpl::total_length() {
  QuicByteCount length = 0;
  for (size_t i = 0; i < num_buffers_; ++i) {
    length += lengths_[i];
  }
  return length;
}

}  // namespace quic
