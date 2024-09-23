// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_

#include <map>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/network_isolation_key.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

class SharedDictionaryManagerOnDisk;
class SimpleUrlPatternMatcher;

// A SharedDictionaryStorage which is managed by SharedDictionaryManagerOnDisk.
class SharedDictionaryStorageOnDisk : public SharedDictionaryStorage {
 public:
  class WrappedDictionaryInfo : public net::SharedDictionaryInfo {
   public:
    WrappedDictionaryInfo(net::SharedDictionaryInfo info,
                          std::unique_ptr<SimpleUrlPatternMatcher> matcher);
    ~WrappedDictionaryInfo();
    WrappedDictionaryInfo(const WrappedDictionaryInfo&) = delete;
    WrappedDictionaryInfo& operator=(const WrappedDictionaryInfo&) = delete;
    WrappedDictionaryInfo(WrappedDictionaryInfo&&);
    WrappedDictionaryInfo& operator=(WrappedDictionaryInfo&&);

    const std::set<mojom::RequestDestination>& match_dest() const {
      return match_dest_;
    }
    const SimpleUrlPatternMatcher* matcher() const { return matcher_.get(); }

   private:
    std::unique_ptr<SimpleUrlPatternMatcher> matcher_;
    std::set<mojom::RequestDestination> match_dest_;
  };

  SharedDictionaryStorageOnDisk(
      base::WeakPtr<SharedDictionaryManagerOnDisk> manager,
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::ScopedClosureRunner on_deleted_closure_runner);

  SharedDictionaryStorageOnDisk(const SharedDictionaryStorageOnDisk&) = delete;
  SharedDictionaryStorageOnDisk& operator=(
      const SharedDictionaryStorageOnDisk&) = delete;

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

  // Called from `SharedDictionaryManagerOnDisk` when dictionary has been
  // deleted.
  void OnDictionaryDeleted(
      const std::set<base::UnguessableToken>& disk_cache_key_tokens);

 protected:
  ~SharedDictionaryStorageOnDisk() override;

 private:
  friend class SharedDictionaryManagerTest;
  friend class SharedDictionaryManagerOnDiskTest;

  void OnDatabaseRead(
      net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError result);
  void OnDictionaryWritten(std::unique_ptr<SimpleUrlPatternMatcher> matcher,
                           net::SharedDictionaryInfo info);
  void OnSharedDictionaryDeleted(
      const base::UnguessableToken& disk_cache_key_token);

  const std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               WrappedDictionaryInfo>>&
  GetDictionaryMapForTesting() {
    return dictionary_info_map_;
  }

  base::WeakPtr<SharedDictionaryManagerOnDisk> manager_;
  const net::SharedDictionaryIsolationKey isolation_key_;
  base::ScopedClosureRunner on_deleted_closure_runner_;
  std::map<
      url::SchemeHostPort,
      std::map<std::tuple<std::string, std::set<mojom::RequestDestination>>,
               WrappedDictionaryInfo>>
      dictionary_info_map_;

  std::map<base::UnguessableToken, raw_ptr<net::SharedDictionary>>
      dictionaries_;

  bool get_dictionary_called_ = false;
  bool is_metadata_ready_ = false;

  std::vector<base::OnceClosure> pending_get_dictionary_tasks_;

  base::WeakPtrFactory<SharedDictionaryStorageOnDisk> weak_factory_{this};
};
}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_
