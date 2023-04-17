// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"

#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"

namespace network {

SharedDictionaryWriterInMemory::SharedDictionaryWriterInMemory(
    FinishCallback finish_callback)
    : finish_callback_(std::move(finish_callback)),
      secure_hash_(crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {}

SharedDictionaryWriterInMemory::~SharedDictionaryWriterInMemory() {
  if (finish_callback_) {
    std::move(finish_callback_)
        .Run(Result::kErrorAborted, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
  }
}

void SharedDictionaryWriterInMemory::Append(const char* buf, int num_bytes) {
  secure_hash_->Update(buf, num_bytes);
  data_.emplace_back(buf, num_bytes);
}

void SharedDictionaryWriterInMemory::Finish() {
  net::SHA256HashValue sha256;
  secure_hash_->Finish(sha256.data, sizeof(sha256.data));

  size_t total_size = 0;
  for (const auto& item : data_) {
    total_size += item.size();
  }

  if (total_size == 0) {
    std::move(finish_callback_)
        .Run(Result::kErrorSizeZero, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size);
  size_t written_size = 0;
  for (const auto& item : data_) {
    memcpy(buffer->data() + written_size, item.c_str(), item.size());
    written_size += item.size();
  }

  std::move(finish_callback_).Run(Result::kSuccess, buffer, total_size, sha256);
}

}  // namespace network
