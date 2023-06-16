// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

#include "base/containers/cxx20_erase_map.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"
#include "url/scheme_host_port.h"

namespace network {

SharedDictionaryStorageInMemory::SharedDictionaryStorageInMemory(
    base::WeakPtr<SharedDictionaryManagerInMemory> manager,
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::ScopedClosureRunner on_deleted_closure_runner)
    : manager_(manager),
      isolation_key_(isolation_key),
      on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {}

SharedDictionaryStorageInMemory::~SharedDictionaryStorageInMemory() = default;

std::unique_ptr<SharedDictionary>
SharedDictionaryStorageInMemory::GetDictionary(const GURL& url) {
  DictionaryInfo* info =
      GetMatchingDictionaryFromDictionaryInfoMap(dictionary_info_map_, url);

  if (!info) {
    return nullptr;
  }
  info->set_last_used_time(base::Time::Now());
  return std::make_unique<SharedDictionaryInMemory>(info->data(), info->size(),
                                                    info->hash());
}

void SharedDictionaryStorageInMemory::DeleteDictionary(
    const url::SchemeHostPort& host,
    const std::string& match) {
  auto it = dictionary_info_map_.find(host);
  if (it != dictionary_info_map_.end()) {
    it->second.erase(match);
    if (it->second.empty()) {
      dictionary_info_map_.erase(it);
    }
  }
}

void SharedDictionaryStorageInMemory::ClearData(
    base::Time start_time,
    base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher) {
  for (auto& it : dictionary_info_map_) {
    base::EraseIf(it.second, [start_time, end_time, url_matcher](auto& it2) {
      const DictionaryInfo& dict = it2.second;
      return (dict.response_time() >= start_time) &&
             (dict.response_time() < end_time) &&
             (!url_matcher || url_matcher.Run(dict.url().GetWithEmptyPath()));
    });
  }
  base::EraseIf(dictionary_info_map_,
                [](auto& it) { return it.second.empty(); });
}

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorageInMemory::CreateWriter(const GURL& url,
                                              base::Time response_time,
                                              base::TimeDelta expiration,
                                              const std::string& match) {
  return base::MakeRefCounted<SharedDictionaryWriterInMemory>(base::BindOnce(
      &SharedDictionaryStorageInMemory::OnDictionaryWritten,
      weak_factory_.GetWeakPtr(), url, response_time, expiration, match));
}

void SharedDictionaryStorageInMemory::OnDictionaryWritten(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    SharedDictionaryWriterInMemory::Result result,
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& hash) {
  if (result != SharedDictionaryWriterInMemory::Result::kSuccess) {
    return;
  }
  dictionary_info_map_[url::SchemeHostPort(url)].insert(std::make_pair(
      match,
      DictionaryInfo(url, response_time, expiration, match,
                     /*last_used_time=*/base::Time::Now(), data, size, hash)));
  if (manager_) {
    manager_->MaybeRunCacheEvictionPerSite(isolation_key_.top_frame_site());
    manager_->MaybeRunCacheEviction();
  }
}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    base::Time last_used_time,
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& hash)
    : url_(url),
      response_time_(response_time),
      expiration_(expiration),
      match_(match),
      last_used_time_(last_used_time),
      data_(std::move(data)),
      size_(size),
      hash_(hash) {}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    DictionaryInfo&& other) = default;

SharedDictionaryStorageInMemory::DictionaryInfo&
SharedDictionaryStorageInMemory::DictionaryInfo::operator=(
    SharedDictionaryStorageInMemory::DictionaryInfo&& other) = default;

SharedDictionaryStorageInMemory::DictionaryInfo::~DictionaryInfo() = default;

}  // namespace network
