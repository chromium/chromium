// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"

#include <limits>

#include "base/containers/span_writer.h"
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

void SharedDictionaryWriterInMemory::Append(base::span<const uint8_t> data) {
  if (!finish_callback_) {
    return;
  }
  base::CheckedNumeric<size_t> checked_total_size = total_size_;
  checked_total_size += data.size();
  if (checked_total_size.ValueOrDefault(std::numeric_limits<size_t>::max()) >
      shared_dictionary::GetDictionarySizeLimit()) {
    data_.clear();
    std::move(finish_callback_)
        .Run(Result::kErrorSizeExceedsLimit, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
    return;
  }
  total_size_ = checked_total_size.ValueOrDie();

  secure_hash_->Update(data);
  data_.emplace_back(base::as_string_view(data));
}

void SharedDictionaryWriterInMemory::Finish() {
  if (!finish_callback_) {
    return;
  }

  net::SHA256HashValue sha256;
  secure_hash_->Finish(sha256);

  if (total_size_ == 0) {
    std::move(finish_callback_)
        .Run(Result::kErrorSizeZero, /*buffer=*/nullptr, /*size=*/0u,
             net::SHA256HashValue());
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size_);
  base::SpanWriter<uint8_t> writer(buffer->span());
  for (const auto& item : data_) {
    writer.Write(base::as_byte_span(item));
  }

  base::UmaHistogramCustomCounts(
      "Net.SharedDictionaryWriterInMemory.DictionarySize", total_size_, 1,
      100000000, 50);

  std::move(finish_callback_)
      .Run(Result::kSuccess, buffer, total_size_, sha256);
}

}  // namespace network
