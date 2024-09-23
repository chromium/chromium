// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ON_DISK_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "net/base/hash_value.h"
#include "net/disk_cache/disk_cache.h"
#include "net/shared_dictionary/shared_dictionary.h"

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace network {

class SharedDictionaryDiskCache;

// A SharedDictionary that can be retrieved from the SharedDictionaryDiskCache.
// This class starts loading the disk cache entry in the constructor. So
// ReadAll() may synchronously return OK if the data has been loaded into memory
// when the method is called.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryOnDisk
    : public net::SharedDictionary {
 public:
  SharedDictionaryOnDisk(size_t size,
                         const net::SHA256HashValue& hash,
                         const std::string& id,
                         const base::UnguessableToken& disk_cache_key_token,
                         SharedDictionaryDiskCache& disk_cahe,
                         base::OnceClosure disk_cache_error_callback,
                         base::ScopedClosureRunner on_deleted_closure_runner);

  // net::SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override;
  scoped_refptr<net::IOBuffer> data() const override;
  size_t size() const override;
  const net::SHA256HashValue& hash() const override;
  const std::string& id() const override;

 private:
  ~SharedDictionaryOnDisk() override;

  enum class State { kLoading, kDone, kFailed };

  void OnEntry(base::Time open_start_time, disk_cache::EntryResult result);
  void OnDataRead(base::Time read_start_time, int result);

  void SetState(State state);

  State state_ = State::kLoading;

  const size_t size_;
  const net::SHA256HashValue hash_;
  const std::string id_;
  base::OnceClosure disk_cache_error_callback_;

  std::vector<base::OnceCallback<void(int)>> readall_callbacks_;
  disk_cache::ScopedEntryPtr entry_;
  scoped_refptr<net::IOBufferWithSize> data_;

  base::ScopedClosureRunner on_deleted_closure_runner_;
  base::WeakPtrFactory<SharedDictionaryOnDisk> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ON_DISK_H_
