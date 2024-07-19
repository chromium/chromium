// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"

#include <limits>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"

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
  if (!finish_callback_) {
    return;
  }
  base::CheckedNumeric<size_t> checked_total_size = total_size_;
  checked_total_size += num_bytes;
  if (checked_total_size.ValueOrDefault(std::numeric_limits<size_t>::max()) >
      shared_dictionary::GetDictionarySizeLimit()) {
    data_.clear();
    std::move(finish_callback_)
        .Run(Result::kErrorSizeExceedsLimit, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
    return;
  }
  total_size_ = checked_total_size.ValueOrDie();

  secure_hash_->Update(buf, num_bytes);
  data_.emplace_back(buf, num_bytes);
}

void SharedDictionaryWriterInMemory::Finish() {
  if (!finish_callback_) {
    return;
  }

  net::SHA256HashValue sha256;
  secure_hash_->Finish(sha256.data, sizeof(sha256.data));

  if (total_size_ == 0) {
    std::move(finish_callback_)
        .Run(Result::kErrorSizeZero, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size_);
  size_t written_size = 0;
  for (const auto& item : data_) {
    memcpy(buffer->data() + written_size, item.c_str(), item.size());
    written_size += item.size();
  }

  base::UmaHistogramCustomCounts(
      "Net.SharedDictionaryWriterInMemory.DictionarySize", total_size_, 1,
      100000000, 50);

  std::move(finish_callback_)
      .Run(Result::kSuccess, buffer, total_size_, sha256);
}

}  // namespace network
