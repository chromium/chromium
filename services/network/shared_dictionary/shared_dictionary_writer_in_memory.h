// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_IN_MEMORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "crypto/secure_hash.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"

namespace net {
class IOBuffer;
struct SHA256HashValue;
}  // namespace net

namespace network {

// A SharedDictionaryWriter which stores the dictionary in memory. This can be
// obtained using SharedDictionaryStorageInMemory::MaybeCreateWriter().
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryWriterInMemory
    : public SharedDictionaryWriter {
 public:
  enum class Result {
    kSuccess,
    kErrorAborted,
    kErrorSizeZero,
    kErrorSizeExceedsLimit,
  };
  using FinishCallback =
      base::OnceCallback<void(Result result,
                              scoped_refptr<net::IOBuffer> buffer,
                              size_t size,
                              const net::SHA256HashValue& hash)>;

  // `finish_callback` is called when the entire dictionary binary has been
  // stored to the memory, or when an error occurs.
  explicit SharedDictionaryWriterInMemory(FinishCallback finish_callback);

  // SharedDictionaryWriter
  void Append(const char* buf, int num_bytes) override;
  void Finish() override;

 private:
  ~SharedDictionaryWriterInMemory() override;

  FinishCallback finish_callback_;
  std::unique_ptr<crypto::SecureHash> secure_hash_;
  std::vector<std::string> data_;
  size_t total_size_ = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_IN_MEMORY_H_
