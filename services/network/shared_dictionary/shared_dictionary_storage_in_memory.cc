// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

#include <map>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"
#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"
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

scoped_refptr<net::SharedDictionary>
SharedDictionaryStorageInMemory::GetDictionarySync(
    const GURL& url,
    mojom::RequestDestination destination) {
  DictionaryInfo* info = GetMatchingDictionaryFromDictionaryInfoMap(
      dictionary_info_map_, url, destination);

  if (!info) {
    return nullptr;
  }

  if (info->response_time() + info->expiration() <= base::Time::Now()) {
    DeleteDictionary(url::SchemeHostPort(info->url()), info->match(),
                     info->match_dest());
    return nullptr;
  }
  info->set_last_used_time(base::Time::Now());
  return info->dictionary();
}

void SharedDictionaryStorageInMemory::GetDictionary(
    const GURL& url,
    mojom::RequestDestination destination,
    base::OnceCallback<void(scoped_refptr<net::SharedDictionary>)> callback) {
  std::move(callback).Run(GetDictionarySync(url, destination));
}

void SharedDictionaryStorageInMemory::DeleteDictionary(
    const url::SchemeHostPort& host,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest) {
  auto it = dictionary_info_map_.find(host);
  if (it != dictionary_info_map_.end()) {
    it->second.erase(std::make_tuple(match, match_dest));
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
    std::erase_if(it.second, [start_time, end_time, url_matcher](auto& it2) {
      const DictionaryInfo& dict = it2.second;
      return (dict.response_time() >= start_time) &&
             (dict.response_time() < end_time) &&
             (!url_matcher || url_matcher.Run(dict.url().GetWithEmptyPath()));
    });
  }
  std::erase_if(dictionary_info_map_,
                [](auto& it) { return it.second.empty(); });
}

void SharedDictionaryStorageInMemory::ClearAllDictionaries() {
  dictionary_info_map_.clear();
}

bool SharedDictionaryStorageInMemory::HasDictionaryBetween(
    base::Time start_time,
    base::Time end_time) {
  for (const auto& [scheme_host_port, info_map] : dictionary_info_map_) {
    std::ignore = scheme_host_port;
    for (const auto& [match, dict] : info_map) {
      std::ignore = match;
      if ((dict.response_time() >= start_time) &&
          (dict.response_time() < end_time)) {
        return true;
      }
    }
  }
  return false;
}

base::expected<scoped_refptr<SharedDictionaryWriter>,
               mojom::SharedDictionaryError>
SharedDictionaryStorageInMemory::CreateWriter(
    const GURL& url,
    base::Time last_fetch_time,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    std::unique_ptr<SimpleUrlPatternMatcher> matcher) {
  CHECK(matcher);
  return base::MakeRefCounted<SharedDictionaryWriterInMemory>(base::BindOnce(
      &SharedDictionaryStorageInMemory::OnDictionaryWritten,
      weak_factory_.GetWeakPtr(), url, last_fetch_time, response_time,
      expiration, match, std::move(matcher), match_dest, id));
}

bool SharedDictionaryStorageInMemory::UpdateLastFetchTimeIfAlreadyRegistered(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    base::Time last_fetch_time) {
  DictionaryInfo* matched_info = FindRegisteredInDictionaryInfoMap(
      dictionary_info_map_, url, response_time, expiration, match, match_dest,
      id);
  if (matched_info) {
    matched_info->set_last_fetch_time(last_fetch_time);
    return true;
  }
  return false;
}

void SharedDictionaryStorageInMemory::OnDictionaryWritten(
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
    const net::SHA256HashValue& hash) {
  if (result != SharedDictionaryWriterInMemory::Result::kSuccess) {
    return;
  }
  dictionary_info_map_[url::SchemeHostPort(url)].insert_or_assign(
      std::make_tuple(match, match_dest),
      DictionaryInfo(url, last_fetch_time, response_time, expiration, match,
                     match_dest, id,
                     /*last_used_time=*/base::Time::Now(), data, size, hash,
                     std::move(matcher)));
  if (manager_) {
    manager_->MaybeRunCacheEvictionPerSite(isolation_key_.top_frame_site());
    manager_->MaybeRunCacheEviction();
  }
}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    const GURL& url,
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
    std::unique_ptr<SimpleUrlPatternMatcher> matcher)
    : url_(url),
      last_fetch_time_(last_fetch_time),
      response_time_(response_time),
      expiration_(expiration),
      match_(match),
      match_dest_(std::move(match_dest)),
      last_used_time_(last_used_time),
      matcher_(std::move(matcher)),
      dictionary_(base::MakeRefCounted<SharedDictionaryInMemory>(data,
                                                                 size,
                                                                 hash,
                                                                 id)) {}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    DictionaryInfo&& other) = default;

SharedDictionaryStorageInMemory::DictionaryInfo&
SharedDictionaryStorageInMemory::DictionaryInfo::operator=(
    SharedDictionaryStorageInMemory::DictionaryInfo&& other) = default;

SharedDictionaryStorageInMemory::DictionaryInfo::~DictionaryInfo() = default;

}  // namespace network
