// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_ON_DISK_H_

#include <deque>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"

namespace net {
class StringIOBuffer;
}  // namespace net

namespace network {

class SharedDictionaryDiskCache;

// A SharedDictionaryWriter which stores the binary of dictionary as an entry of
// SharedDictionaryDiskCache.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryWriterOnDisk
    : public SharedDictionaryWriter {
 public:
  enum class Result {
    kSuccess,
    kErrorCreateEntryFailed,
    kErrorWriteDataFailed,
    kErrorAborted,
    kErrorSizeZero,
    kErrorSizeExceedsLimit,
  };
  using FinishCallback = base::OnceCallback<
      void(Result result, size_t size, const net::SHA256HashValue& hash)>;
  // `callback` is called when the entire dictionary binary has been stored to
  // the disk cache, or when an error occurs.
  SharedDictionaryWriterOnDisk(
      const base::UnguessableToken& token,
      FinishCallback callback,
      base::WeakPtr<SharedDictionaryDiskCache> disk_cahe);
  void Initialize();

  // SharedDictionaryWriter
  void Append(const char* buf, int num_bytes) override;
  void Finish() override;

 private:
  enum class State { kBeforeInitialize, kInitializing, kInitialized, kFailed };

  ~SharedDictionaryWriterOnDisk() override;

  void OnEntry(disk_cache::EntryResult result);
  void WriteData(scoped_refptr<net::StringIOBuffer> buffer);
  void OnWrittenData(int expected_result, int result);

  void OnFailed(Result result);

  void MaybeFinish();

  const base::UnguessableToken token_;
  FinishCallback callback_;
  base::WeakPtr<SharedDictionaryDiskCache> disk_cahe_;
  std::unique_ptr<crypto::SecureHash> secure_hash_;

  size_t total_size_ = 0;
  size_t written_size_ = 0;

  State state_ = State::kBeforeInitialize;
  disk_cache::ScopedEntryPtr entry_;
  std::deque<scoped_refptr<net::StringIOBuffer>> pending_write_buffers_;

  int offset_ = 0;
  bool finish_called_ = false;
  int writing_count_ = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_ON_DISK_H_
