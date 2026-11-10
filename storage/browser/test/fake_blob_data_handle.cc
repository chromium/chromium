// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob_data_handle.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/net_errors.h"

namespace storage {

FakeBlobDataHandle::FakeBlobDataHandle(std::string body_data)
    : body_data_(std::move(body_data)) {}

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

  base::span<const uint8_t> bytes = base::as_byte_span(body_data_);
  bytes = bytes.subspan(static_cast<size_t>(src_offset));
  bytes = bytes.first(base::checked_cast<size_t>(bytes_to_read));
  MojoResult result = producer->WriteAllData(bytes);

  // This should all succeed.
  DCHECK_EQ(MOJO_RESULT_OK, result);

  // The callback expects a `net::Error` only and not the number of bytes read.
  std::move(callback).Run(net::OK);
}

FakeBlobDataHandle::~FakeBlobDataHandle() = default;

}  // namespace storage
