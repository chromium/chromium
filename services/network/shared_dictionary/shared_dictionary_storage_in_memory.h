// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_IN_MEMORY_H_

#include <map>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace network {
namespace cors {
class CorsURLLoaderSharedDictionaryTest;
}  // namespace cors

class SharedDictionaryManagerInMemory;

// A SharedDictionaryStorage which is managed by
// SharedDictionaryManagerInMemory.
class SharedDictionaryStorageInMemory : public SharedDictionaryStorage {
 public:
  // This class is used to keep the dictionary information in memory.
  class DictionaryInfo {
   public:
    DictionaryInfo(const GURL& url,
                   base::Time response_time,
                   base::TimeDelta expiration,
                   const std::string& match,
                   base::Time last_used_time,
                   scoped_refptr<net::IOBuffer> data,
                   size_t size,
                   const net::SHA256HashValue& hash);

    DictionaryInfo(const DictionaryInfo&) = delete;
    DictionaryInfo& operator=(const DictionaryInfo&) = delete;

    DictionaryInfo(DictionaryInfo&& other);
    DictionaryInfo& operator=(DictionaryInfo&& other);

    ~DictionaryInfo();

    const GURL& url() const { return url_; }
    const base::Time& response_time() const { return response_time_; }
    base::TimeDelta expiration() const { return expiration_; }
    const std::string& match() const { return match_; }
    const base::Time& last_used_time() const { return last_used_time_; }
    const scoped_refptr<net::IOBuffer>& data() const { return data_; }
    size_t size() const { return size_; }
    const net::SHA256HashValue& hash() const { return hash_; }

    void set_last_used_time(base::Time last_used_time) {
      last_used_time_ = last_used_time;
    }

   private:
    GURL url_;
    base::Time response_time_;
    base::TimeDelta expiration_;
    std::string match_;
    base::Time last_used_time_;
    scoped_refptr<net::IOBuffer> data_;
    size_t size_;
    net::SHA256HashValue hash_;
  };

  SharedDictionaryStorageInMemory(
      base::WeakPtr<SharedDictionaryManagerInMemory> manager,
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::ScopedClosureRunner on_deleted_closure_runner);

  SharedDictionaryStorageInMemory(const SharedDictionaryStorageInMemory&) =
      delete;
  SharedDictionaryStorageInMemory& operator=(
      const SharedDictionaryStorageInMemory&) = delete;

  // SharedDictionaryStorage
  std::unique_ptr<SharedDictionary> GetDictionary(const GURL& url) override;

  const std::map<url::SchemeHostPort, std::map<std::string, DictionaryInfo>>&
  GetDictionaryMap() {
    return dictionary_info_map_;
  }

  void DeleteDictionary(const url::SchemeHostPort& host,
                        const std::string& match);
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher);

 private:
  friend class SharedDictionaryManagerTest;
  friend class network::cors::CorsURLLoaderSharedDictionaryTest;
  ~SharedDictionaryStorageInMemory() override;

  // SharedDictionaryStorage
  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match) override;

  // Called when SharedDictionaryWriterInMemory::Finish() is called.
  void OnDictionaryWritten(const GURL& url,
                           base::Time response_time,
                           base::TimeDelta expiration,
                           const std::string& match,
                           SharedDictionaryWriterInMemory::Result result,
                           scoped_refptr<net::IOBuffer> data,
                           size_t size,
                           const net::SHA256HashValue& hash);

  base::WeakPtr<SharedDictionaryManagerInMemory> manager_;
  const net::SharedDictionaryIsolationKey isolation_key_;
  base::ScopedClosureRunner on_deleted_closure_runner_;

  std::map<url::SchemeHostPort, std::map<std::string, DictionaryInfo>>
      dictionary_info_map_;
  base::WeakPtrFactory<SharedDictionaryStorageInMemory> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_
