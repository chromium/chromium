// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_STORAGE_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_STORAGE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/quic/platform/impl/quic_iovec_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice_span.h"

namespace quic {

class QUIC_EXPORT_PRIVATE QuicMemSliceStorageImpl {
 public:
  QuicMemSliceStorageImpl(const struct iovec* iov,
                          int iov_count,
                          QuicBufferAllocator* allocator,
                          const QuicByteCount max_slice_len);

  QuicMemSliceStorageImpl(const QuicMemSliceStorageImpl& other);
  QuicMemSliceStorageImpl& operator=(const QuicMemSliceStorageImpl& other);
  QuicMemSliceStorageImpl(QuicMemSliceStorageImpl&& other);
  QuicMemSliceStorageImpl& operator=(QuicMemSliceStorageImpl&& other);

  ~QuicMemSliceStorageImpl();

  QuicMemSliceSpan ToSpan() {
    return QuicMemSliceSpan(QuicMemSliceSpanImpl(
        buffers_.data(), lengths_.data(), buffers_.size()));
  }

  void Append(QuicMemSliceImpl mem_slice);

 private:
  std::vector<scoped_refptr<net::IOBuffer>> buffers_;
  std::vector<size_t> lengths_;
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_MEM_SLICE_STORAGE_IMPL_H_
