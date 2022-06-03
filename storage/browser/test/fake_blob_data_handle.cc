// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob_data_handle.h"

#include "base/bind.h"
#include "net/base/net_errors.h"

namespace storage {

FakeBlobDataHandle::FakeBlobDataHandle(std::string body_data,
                                       std::string side_data)
    : body_data_(std::move(body_data)), side_data_(std::move(side_data)) {}

uint64_t FakeBlobDataHandle::GetSize() const {
  return body_data_.size();
}

void FakeBlobDataHandle::Read(mojo::ScopedDataPipeProducerHandle producer,
                              uint64_t src_offset,
                              uint64_t bytes_to_read,
                              base::OnceCallback<void(int)> callback) {
  if (src_offset >= body_data_.size()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  uint32_t num_bytes = bytes_to_read;
  uint32_t orig_num_bytes = num_bytes;
  MojoResult result =
      producer->WriteData(body_data_.c_str() + src_offset, &num_bytes,
                          MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);

  // This should all succeed.
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK_EQ(orig_num_bytes, num_bytes);

  std::move(callback).Run(num_bytes);
}

uint64_t FakeBlobDataHandle::GetSideDataSize() const {
  return side_data_.size();
}

void FakeBlobDataHandle::ReadSideData(
    base::OnceCallback<void(int, mojo_base::BigBuffer)> callback) {
  if (side_data_.size() == 0) {
    std::move(callback).Run(side_data_.size(), mojo_base::BigBuffer());
    return;
  }

  mojo_base::BigBuffer buffer(side_data_.size());
  memcpy(buffer.data(), side_data_.data(), side_data_.size());

  std::move(callback).Run(side_data_.size(), std::move(buffer));
}

FakeBlobDataHandle::~FakeBlobDataHandle() = default;

}  // namespace storage
