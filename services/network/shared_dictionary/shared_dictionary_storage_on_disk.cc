// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "net/base/io_buffer.h"
#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"
#include "url/scheme_host_port.h"

namespace network {

// This is a RefCounted subclass of SharedDictionaryOnDisk. This is used to
// share a SharedDictionaryOnDisk for multiple concurrent network requests.
class SharedDictionaryStorageOnDisk::RefCountedSharedDictionary
    : public SharedDictionaryOnDisk,
      public base::RefCounted<RefCountedSharedDictionary> {
 public:
  // `on_deleted_closure_runner` will be called when `this` is deleted.
  RefCountedSharedDictionary(
      size_t size,
      const net::SHA256HashValue& hash,
      const base::UnguessableToken& disk_cache_key_token,
      SharedDictionaryDiskCache& disk_cahe,
      base::ScopedClosureRunner on_deleted_closure_runner)
      : SharedDictionaryOnDisk(size, hash, disk_cache_key_token, &disk_cahe),
        on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {}

 private:
  friend class RefCounted<RefCountedSharedDictionary>;
  ~RefCountedSharedDictionary() override = default;

  base::ScopedClosureRunner on_deleted_closure_runner_;
};

// This is a subclass of SharedDictionaryOnDisk. This holds a reference to a
// RefCountedSharedDictionary.
class SharedDictionaryStorageOnDisk::WrappedSharedDictionary
    : public SharedDictionary {
 public:
  explicit WrappedSharedDictionary(
      scoped_refptr<RefCountedSharedDictionary> ref_counted_shared_dictionary)
      : ref_counted_shared_dictionary_(
            std::move(ref_counted_shared_dictionary)) {}

  WrappedSharedDictionary(const WrappedSharedDictionary&) = delete;
  WrappedSharedDictionary& operator=(const WrappedSharedDictionary&) = delete;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    return ref_counted_shared_dictionary_->ReadAll(std::move(callback));
  }
  scoped_refptr<net::IOBuffer> data() const override {
    return ref_counted_shared_dictionary_->data();
  }
  size_t size() const override {
    return ref_counted_shared_dictionary_->size();
  }
  const net::SHA256HashValue& hash() const override {
    return ref_counted_shared_dictionary_->hash();
  }

 private:
  scoped_refptr<RefCountedSharedDictionary> ref_counted_shared_dictionary_;
};

SharedDictionaryStorageOnDisk::SharedDictionaryStorageOnDisk(
    base::WeakPtr<SharedDictionaryManagerOnDisk> manager,
    const net::SharedDictionaryStorageIsolationKey& isolation_key,
    base::ScopedClosureRunner on_deleted_closure_runner)
    : manager_(manager),
      isolation_key_(isolation_key),
      on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {
  manager_->metadata_store().GetDictionaries(
      isolation_key_,
      base::BindOnce(&SharedDictionaryStorageOnDisk::OnDatabaseRead,
                     weak_factory_.GetWeakPtr()));
}

SharedDictionaryStorageOnDisk::~SharedDictionaryStorageOnDisk() = default;

std::unique_ptr<SharedDictionary> SharedDictionaryStorageOnDisk::GetDictionary(
    const GURL& url) {
  if (!manager_) {
    return nullptr;
  }
  const net::SharedDictionaryInfo* info =
      GetMatchingDictionaryFromDictionaryInfoMap(dictionary_info_map_, url);
  if (!info) {
    return nullptr;
  }
  auto it = dictionaries_.find(info->disk_cache_key_token());
  if (it != dictionaries_.end()) {
    CHECK_EQ(info->size(), it->second->size());
    CHECK(info->hash() == it->second->hash());
    return std::make_unique<WrappedSharedDictionary>(it->second.get());
  }

  auto ref_counted_shared_dictionary = base::MakeRefCounted<
      RefCountedSharedDictionary>(
      info->size(), info->hash(), info->disk_cache_key_token(),
      manager_->disk_cache(),
      base::ScopedClosureRunner(base::BindOnce(
          &SharedDictionaryStorageOnDisk::OnRefCountedSharedDictionaryDeleted,
          weak_factory_.GetWeakPtr(), info->disk_cache_key_token())));
  dictionaries_.emplace(info->disk_cache_key_token(),
                        ref_counted_shared_dictionary.get());
  return std::make_unique<WrappedSharedDictionary>(
      std::move(ref_counted_shared_dictionary));
}

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorageOnDisk::CreateWriter(const GURL& url,
                                            base::Time response_time,
                                            base::TimeDelta expiration,
                                            const std::string& match) {
  if (!manager_) {
    return nullptr;
  }
  auto writer = base::MakeRefCounted<SharedDictionaryWriterOnDisk>(
      base::BindOnce(
          &SharedDictionaryStorageOnDisk::OnDictionaryWrittenInDiskCache,
          weak_factory_.GetWeakPtr(), url, response_time, expiration, match),
      manager_->disk_cache().GetWeakPtr());
  writer->Initialize();
  return writer;
}

void SharedDictionaryStorageOnDisk::OnDatabaseRead(
    net::SQLitePersistentSharedDictionaryStore::Error error,
    std::vector<net::SharedDictionaryInfo> info_list) {
  CHECK(dictionary_info_map_.empty());
  std::set<base::UnguessableToken> deleted_cache_tokens;
  for (auto& info : info_list) {
    const url::SchemeHostPort scheme_host_port =
        url::SchemeHostPort(info.url());
    const std::string match = info.match();
    (dictionary_info_map_[scheme_host_port])
        .insert(std::make_pair(match, std::move(info)));
  }
}

void SharedDictionaryStorageOnDisk::OnDictionaryWrittenInDiskCache(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    SharedDictionaryWriterOnDisk::Result result,
    size_t size,
    const net::SHA256HashValue& hash,
    const base::UnguessableToken& disk_cache_key_token) {
  if (!manager_) {
    return;
  }
  if (result != SharedDictionaryWriterOnDisk::Result::kSuccess) {
    return;
  }
  base::Time last_used_time = base::Time::Now();
  net::SharedDictionaryInfo info(url, response_time, expiration, match,
                                 last_used_time, size, hash,
                                 disk_cache_key_token,
                                 /*primary_key_in_database=*/absl::nullopt);

  manager_->metadata_store().RegisterDictionary(
      isolation_key_, info,
      base::BindOnce(
          &SharedDictionaryStorageOnDisk::OnDictionaryWrittenInDatabase,
          weak_factory_.GetWeakPtr(), info));
}

void SharedDictionaryStorageOnDisk::OnDictionaryWrittenInDatabase(
    net::SharedDictionaryInfo info,
    net::SQLitePersistentSharedDictionaryStore::Error error,
    absl::optional<int64_t> primary_key_in_database) {
  if (!manager_) {
    return;
  }
  if (error != net::SQLitePersistentSharedDictionaryStore::Error::kOk) {
    manager_->disk_cache().DoomEntry(info.disk_cache_key_token().ToString(),
                                     base::DoNothing());
    return;
  }

  CHECK(primary_key_in_database.has_value());
  info.set_primary_key_in_database(*primary_key_in_database);
  url::SchemeHostPort scheme_host_port = url::SchemeHostPort(info.url());
  auto it1 = dictionary_info_map_.find(scheme_host_port);
  if (it1 == dictionary_info_map_.end()) {
    dictionary_info_map_.insert(make_pair(
        scheme_host_port, std::map<std::string, net::SharedDictionaryInfo>(
                              {{info.match(), info}})));
    return;
  }

  std::map<std::string, net::SharedDictionaryInfo>& match_info_map =
      it1->second;
  auto it2 = match_info_map.find(info.match());
  if (it2 == match_info_map.end()) {
    match_info_map.insert(make_pair(info.match(), info));
    return;
  }

  manager_->disk_cache().DoomEntry(
      it2->second.disk_cache_key_token().ToString(), base::DoNothing());
  it2->second = info;
}

void SharedDictionaryStorageOnDisk::OnRefCountedSharedDictionaryDeleted(
    const base::UnguessableToken& disk_cache_key_token) {
  size_t removed_count = dictionaries_.erase(disk_cache_key_token);
  CHECK_EQ(1U, removed_count);
}

}  // namespace network
