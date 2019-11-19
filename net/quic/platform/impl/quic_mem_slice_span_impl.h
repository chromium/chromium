// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

// QuicMemSliceSpanImpl wraps a MemSlice span.
class QUIC_EXPORT_PRIVATE QuicMemSliceSpanImpl {
 public:
  QuicMemSliceSpanImpl(const scoped_refptr<net::IOBuffer>* buffers,
                       const size_t* lengths,
                       size_t num_buffers);
  explicit QuicMemSliceSpanImpl(QuicMemSliceImpl* slice);

  QuicMemSliceSpanImpl(const QuicMemSliceSpanImpl& other);
  QuicMemSliceSpanImpl& operator=(const QuicMemSliceSpanImpl& other);
  QuicMemSliceSpanImpl(QuicMemSliceSpanImpl&& other);
  QuicMemSliceSpanImpl& operator=(QuicMemSliceSpanImpl&& other);

  ~QuicMemSliceSpanImpl();

  QuicStringPiece GetData(size_t index) {
    return QuicStringPiece(buffers_[index]->data(), lengths_[index]);
  }

  template <typename ConsumeFunction>
  QuicByteCount ConsumeAll(ConsumeFunction consume) {
    size_t saved_length = 0;
    for (size_t i = 0; i < num_buffers_; ++i) {
      if (lengths_[i] == 0) {
        // Skip empty buffer.
        continue;
      }
      saved_length += lengths_[i];

      consume(QuicMemSlice(QuicMemSliceImpl(buffers_[i], lengths_[i])));
    }
    return saved_length;
  }

  QuicByteCount total_length();

  size_t NumSlices() { return num_buffers_; }

  bool empty() const { return num_buffers_ == 0; }

 private:
  const scoped_refptr<net::IOBuffer>* buffers_;
  const size_t* lengths_;
  // Not const so that the move operator can work properly.
  size_t num_buffers_;
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_SPAN_IMPL_H_
