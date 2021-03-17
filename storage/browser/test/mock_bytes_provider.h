// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_BYTES_PROVIDER_H_
#define STORAGE_BROWSER_TEST_MOCK_BYTES_PROVIDER_H_

#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"

namespace storage {

// Mock BytesProvider implementation. RequestAsStream blocks, so make sure to
// bind this implementation to a pipe on a separate sequence from where the
// bytes are consumed.
class MockBytesProvider : public blink::mojom::BytesProvider {
 public:
  explicit MockBytesProvider(
      std::vector<uint8_t> data,
      size_t* reply_request_count = nullptr,
      size_t* stream_request_count = nullptr,
      size_t* file_request_count = nullptr,
      base::Optional<base::Time> file_modification_time = base::Time());
  ~MockBytesProvider() override;

  // BytesProvider implementation:
  void RequestAsReply(RequestAsReplyCallback callback) override;
  void RequestAsStream(mojo::ScopedDataPipeProducerHandle pipe) override;
  void RequestAsFile(uint64_t source_offset,
                     uint64_t source_size,
                     base::File file,
                     uint64_t file_offset,
                     RequestAsFileCallback callback) override;

 private:
  std::vector<uint8_t> data_;
  size_t* reply_request_count_;
  size_t* stream_request_count_;
  size_t* file_request_count_;
  base::Optional<base::Time> file_modification_time_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_BYTES_PROVIDER_H_
