// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_IN_MEMORY_H_

#include <map>
#include <optional>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"
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

class SharedDictionaryInMemory;
class SharedDictionaryManagerInMemory;
class SimpleUrlPatternMatcher;

// A SharedDictionaryStorage which is managed by
// SharedDictionaryManagerInMemory.
class SharedDictionaryStorageInMemory : public SharedDictionaryStorage {
 public:
  // This class is used to keep the dictionary information in memory.
  class DictionaryInfo {
   public:
    DictionaryInfo(const GURL& url,
                   base::Time last_fetch_time,
                   base::Time response_time,
                   base::TimeDelta expiration,
                   const std::string& match,
                   std::set<mojom::RequestDestination> match_dest,
                   const std::string& id,
                   base::Time last_used_time,
                   scoped_refptr<net::IOBuffer> data,
                   size_t size,
                   const net::SHA256HashValue& hash,
                   std::unique_ptr<SimpleUrlPatternMatcher> matcher);

    DictionaryInfo(const DictionaryInfo&) = delete;
    DictionaryInfo& operator=(const DictionaryInfo&) = delete;

    DictionaryInfo(DictionaryInfo&& other);
    DictionaryInfo& operator=(DictionaryInfo&& other);

    ~DictionaryInfo();

    const GURL& url() const { return url_; }
    const base::Time& last_fetch_time() const { return last_fetch_time_; }
    const base::Time& response_time() const { return response_time_; }
    base::TimeDelta expiration() const { return expiration_; }
    const std::string& match() const { return match_; }
    const std::set<mojom::RequestDestination>& match_dest() const {
      return match_dest_;
    }
    const std::string& id() const { return dictionary_->id(); }
    const base::Time& last_used_time() const { return last_used_time_; }
    size_t size() const { return dictionary_->size(); }
    const net::SHA256HashValue& hash() const { return dictionary_->hash(); }
    const SimpleUrlPatternMatcher* matcher() const { return matcher_.get(); }

    void set_last_fetch_time(base::Time last_fetch_time) {
      last_fetch_time_ = last_fetch_time;
    }
    void set_last_used_time(base::Time last_used_time) {
      last_used_time_ = last_used_time;
    }

    scoped_refptr<SharedDictionaryInMemory> dictionary() const {
      return dictionary_;
    }

   private:
    GURL url_;
    base::Time last_fetch_time_;
    base::Time response_time_;
    base::TimeDelta expiration_;
    std::string match_;
    std::set<mojom::RequestDestination> match_dest_;
    base::Time last_used_time_;
    std::unique_ptr<SimpleUrlPatternMatcher> matcher_;

    scoped_refptr<SharedDictionaryInMemory> dictionary_;
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
  scoped_refptr<net::SharedDictionary> GetDictionarySync(
      const GURL& url,
      mojom::RequestDestination destination) override;
  void GetDictionary(
      const GURL& url,
      mojom::RequestDestination destination,
      base::OnceCallback<void(scoped_refptr<net::SharedDictionary>)> callback)
      override;
  base::expected<scoped_refptr<SharedDictionaryWriter>,
                 mojom::SharedDictionaryError>
  CreateWriter(const GURL& url,
               base::Time last_fetch_time,
               base::Time response_time,
               base::TimeDelta expiration,
               const std::string& match,
               const std::set<mojom::RequestDestination>& match_dest,
               const std::string& id,
               std::unique_ptr<SimpleUrlPatternMatcher> matcher) override;
  bool UpdateLastFetchTimeIfAlreadyRegistered(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      const std::set<mojom::RequestDestination>& match_dest,
      const std::string& id,
      base::Time last_fetch_time) override;

  const std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               DictionaryInfo>>&
  GetDictionaryMap() {
    return dictionary_info_map_;
  }

  void DeleteDictionary(const url::SchemeHostPort& host,
                        const std::string& match,
                        const std::set<mojom::RequestDestination>& match_dest);
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher);
  void ClearAllDictionaries();
  bool HasDictionaryBetween(base::Time start_time, base::Time end_time);

 private:
  friend class SharedDictionaryManagerTest;
  friend class network::cors::CorsURLLoaderSharedDictionaryTest;
  ~SharedDictionaryStorageInMemory() override;

  // Called when SharedDictionaryWriterInMemory::Finish() is called.
  void OnDictionaryWritten(
      const GURL& url,
      base::Time last_fetch_time,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      std::unique_ptr<SimpleUrlPatternMatcher> matcher,
      const std::set<mojom::RequestDestination>& match_dest,
      const std::string& id,
      SharedDictionaryWriterInMemory::Result result,
      scoped_refptr<net::IOBuffer> data,
      size_t size,
      const net::SHA256HashValue& hash);

  base::WeakPtr<SharedDictionaryManagerInMemory> manager_;
  const net::SharedDictionaryIsolationKey isolation_key_;
  base::ScopedClosureRunner on_deleted_closure_runner_;

  std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               DictionaryInfo>>
      dictionary_info_map_;
  base::WeakPtrFactory<SharedDictionaryStorageInMemory> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_IN_MEMORY_H_
