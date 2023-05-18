// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_

#include <map>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/network_isolation_key.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/shared_dictionary/shared_dictionary_storage_isolation_key.h"
#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

class SharedDictionaryManagerOnDisk;

// A SharedDictionaryStorage which is managed by SharedDictionaryManagerOnDisk.
class SharedDictionaryStorageOnDisk : public SharedDictionaryStorage {
 public:
  SharedDictionaryStorageOnDisk(
      base::WeakPtr<SharedDictionaryManagerOnDisk> manager,
      const net::SharedDictionaryStorageIsolationKey& isolation_key,
      base::ScopedClosureRunner on_deleted_closure_runner);

  SharedDictionaryStorageOnDisk(const SharedDictionaryStorageOnDisk&) = delete;
  SharedDictionaryStorageOnDisk& operator=(
      const SharedDictionaryStorageOnDisk&) = delete;

  // SharedDictionaryStorage
  std::unique_ptr<SharedDictionary> GetDictionary(const GURL& url) override;
  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match) override;

 protected:
  ~SharedDictionaryStorageOnDisk() override;

 private:
  friend class SharedDictionaryManagerTest;
  friend class SharedDictionaryManagerOnDiskTest;

  class RefCountedSharedDictionary;
  class WrappedSharedDictionary;

  void OnDatabaseRead(
      net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError result);
  void OnDictionaryWritten(net::SharedDictionaryInfo info);
  void OnRefCountedSharedDictionaryDeleted(
      const base::UnguessableToken& disk_cache_key_token);

  const std::map<url::SchemeHostPort,
                 std::map<std::string, net::SharedDictionaryInfo>>&
  GetDictionaryMapForTesting() {
    return dictionary_info_map_;
  }

  base::WeakPtr<SharedDictionaryManagerOnDisk> manager_;
  const net::SharedDictionaryStorageIsolationKey isolation_key_;
  base::ScopedClosureRunner on_deleted_closure_runner_;
  std::map<url::SchemeHostPort,
           std::map<std::string, net::SharedDictionaryInfo>>
      dictionary_info_map_;
  std::map<base::UnguessableToken, raw_ptr<RefCountedSharedDictionary>>
      dictionaries_;

  base::WeakPtrFactory<SharedDictionaryStorageOnDisk> weak_factory_{this};
};
}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ON_DISK_H_
