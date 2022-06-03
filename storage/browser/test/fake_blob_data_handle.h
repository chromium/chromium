// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_
#define STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_

#include <string>

#include "base/callback.h"
#include "storage/browser/blob/blob_data_item.h"

namespace storage {

// All callbacks in the FakeBlobDataHandle are called synchronously.
class FakeBlobDataHandle : public BlobDataItem::DataHandle {
 public:
  FakeBlobDataHandle(std::string body_data, std::string side_data);

  // BlobDataItem::DataHandle implementation.
  uint64_t GetSize() const override;
  void Read(mojo::ScopedDataPipeProducerHandle producer,
            uint64_t src_offset,
            uint64_t bytes_to_read,
            base::OnceCallback<void(int)> callback) override;

  uint64_t GetSideDataSize() const override;
  void ReadSideData(
      base::OnceCallback<void(int, mojo_base::BigBuffer)> callback) override;
  void PrintTo(::std::ostream* os) const override {}

 private:
  ~FakeBlobDataHandle() override;

  const std::string body_data_;
  const std::string side_data_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_
