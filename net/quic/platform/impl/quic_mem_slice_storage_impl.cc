// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_mem_slice_storage_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_buffer_allocator.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"

namespace quic {

QuicMemSliceStorageImpl::QuicMemSliceStorageImpl(
    const struct iovec* iov,
    int iov_count,
    QuicBufferAllocator* allocator,
    const QuicByteCount max_slice_len) {
  if (iov == nullptr) {
    return;
  }
  QuicByteCount write_len = 0;
  for (int i = 0; i < iov_count; ++i) {
    write_len += iov[i].iov_len;
  }
  DCHECK_LT(0u, write_len);

  QuicByteCount iov_offset = 0;
  while (write_len > 0) {
    size_t slice_len = std::min(write_len, max_slice_len);
    auto io_buffer = base::MakeRefCounted<net::IOBuffer>(slice_len);
    QuicUtils::CopyToBuffer(iov, iov_count, iov_offset, slice_len,
                            const_cast<char*>(io_buffer->data()));
    buffers_.push_back(std::move(io_buffer));
    lengths_.push_back(slice_len);
    write_len -= slice_len;
    iov_offset += slice_len;
  }
}

void QuicMemSliceStorageImpl::Append(QuicMemSliceImpl mem_slice) {
  buffers_.push_back(*mem_slice.impl());
  lengths_.push_back(mem_slice.length());
}

QuicMemSliceStorageImpl::QuicMemSliceStorageImpl(
    const QuicMemSliceStorageImpl& other) = default;
QuicMemSliceStorageImpl::QuicMemSliceStorageImpl(
    QuicMemSliceStorageImpl&& other) = default;
QuicMemSliceStorageImpl& QuicMemSliceStorageImpl::operator=(
    const QuicMemSliceStorageImpl& other) = default;
QuicMemSliceStorageImpl& QuicMemSliceStorageImpl::operator=(
    QuicMemSliceStorageImpl&& other) = default;
QuicMemSliceStorageImpl::~QuicMemSliceStorageImpl() = default;

}  // namespace quic
