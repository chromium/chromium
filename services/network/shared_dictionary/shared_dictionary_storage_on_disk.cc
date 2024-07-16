// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_on_disk.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"
#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

void RecordMetadataReadTimeMetrics(
    const net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError&
        result,
    base::TimeDelta time_delta) {
  std::string result_string;
  if (!result.has_value()) {
    result_string = "Failure";
  } else if (result.value().empty()) {
    result_string = "Empty";
  } else {
    result_string = "NonEmpty";
  }
  base::UmaHistogramTimes(
      base::StrCat({"Net.SharedDictionaryStorageOnDisk.MetadataReadTime.",
                    result_string}),
      time_delta);
}

std::set<mojom::RequestDestination> ToRequestDestinationSet(
    std::string_view input) {
  const std::vector<std::string_view> dest_strings = base::SplitStringPiece(
      input, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<mojom::RequestDestination> destinations;
  for (const auto dest_string : dest_strings) {
    auto dest = RequestDestinationFromString(
        dest_string, EmptyRequestDestinationOption::kUseFiveCharEmptyString);
    if (!dest) {
      LOG(ERROR) << "Invalid request destination string: " << dest_string;
      continue;
    }
    destinations.insert(*dest);
  }
  return destinations;
}

}  // namespace

SharedDictionaryStorageOnDisk::WrappedDictionaryInfo::WrappedDictionaryInfo(
    net::SharedDictionaryInfo info,
    std::unique_ptr<SimpleUrlPatternMatcher> matcher)
    : net::SharedDictionaryInfo(std::move(info)),
      matcher_(std::move(matcher)),
      match_dest_(ToRequestDestinationSet(match_dest_string())) {}
SharedDictionaryStorageOnDisk::WrappedDictionaryInfo::~WrappedDictionaryInfo() =
    default;
SharedDictionaryStorageOnDisk::WrappedDictionaryInfo::WrappedDictionaryInfo(
    WrappedDictionaryInfo&&) = default;
SharedDictionaryStorageOnDisk::WrappedDictionaryInfo&
SharedDictionaryStorageOnDisk::WrappedDictionaryInfo::operator=(
    WrappedDictionaryInfo&&) = default;

SharedDictionaryStorageOnDisk::SharedDictionaryStorageOnDisk(
    base::WeakPtr<SharedDictionaryManagerOnDisk> manager,
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::ScopedClosureRunner on_deleted_closure_runner)
    : manager_(manager),
      isolation_key_(isolation_key),
      on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {
  manager_->metadata_store().GetDictionaries(
      isolation_key_,
      base::BindOnce(
          [](base::WeakPtr<SharedDictionaryStorageOnDisk> weak_ptr,
             base::Time start_time,
             net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError
                 result) {
            RecordMetadataReadTimeMetrics(result,
                                          base::Time::Now() - start_time);
            if (weak_ptr) {
              weak_ptr->OnDatabaseRead(std::move(result));
            }
          },
          weak_factory_.GetWeakPtr(), base::Time::Now()));
}

SharedDictionaryStorageOnDisk::~SharedDictionaryStorageOnDisk() = default;

scoped_refptr<net::SharedDictionary>
SharedDictionaryStorageOnDisk::GetDictionarySync(
    const GURL& url,
    mojom::RequestDestination destination) {
  if (!get_dictionary_called_) {
    get_dictionary_called_ = true;
    base::UmaHistogramBoolean(
        "Net.SharedDictionaryStorageOnDisk.IsMetadataReadyOnFirstUse",
        is_metadata_ready_);
  }

  if (!manager_) {
    return nullptr;
  }
  net::SharedDictionaryInfo* info = GetMatchingDictionaryFromDictionaryInfoMap(
      dictionary_info_map_, url, destination);
  if (!info) {
    return nullptr;
  }

  if (info->response_time() + info->expiration() <= base::Time::Now()) {
    manager_->MaybePostExpiredDictionaryDeletionTask();
    return nullptr;
  }

  manager_->UpdateDictionaryLastUsedTime(*info);

  auto it = dictionaries_.find(info->disk_cache_key_token());
  if (it != dictionaries_.end()) {
    CHECK_EQ(info->size(), it->second->size());
    CHECK(info->hash() == it->second->hash());
    return it->second.get();
  }

  auto shared_dictionary = base::MakeRefCounted<SharedDictionaryOnDisk>(
      info->size(), info->hash(), info->id(), info->disk_cache_key_token(),
      manager_->disk_cache(),
      base::BindOnce(
          &SharedDictionaryManagerOnDisk::MaybePostMismatchingEntryDeletionTask,
          manager_),
      base::ScopedClosureRunner(base::BindOnce(
          &SharedDictionaryStorageOnDisk::OnSharedDictionaryDeleted,
          weak_factory_.GetWeakPtr(), info->disk_cache_key_token())));
  dictionaries_.emplace(info->disk_cache_key_token(), shared_dictionary.get());
  return shared_dictionary;
}

void SharedDictionaryStorageOnDisk::GetDictionary(
    const GURL& url,
    mojom::RequestDestination destination,
    base::OnceCallback<void(scoped_refptr<net::SharedDictionary>)> callback) {
  if (is_metadata_ready_) {
    std::move(callback).Run(GetDictionarySync(url, destination));
    return;
  }
  pending_get_dictionary_tasks_.emplace_back(base::BindOnce(
      &SharedDictionaryStorageOnDisk::GetDictionary, weak_factory_.GetWeakPtr(),
      url, destination, std::move(callback)));
}

base::expected<scoped_refptr<SharedDictionaryWriter>,
               mojom::SharedDictionaryError>
SharedDictionaryStorageOnDisk::CreateWriter(
    const GURL& url,
    base::Time last_fetch_time,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    std::unique_ptr<SimpleUrlPatternMatcher> matcher) {
  CHECK(matcher);
  if (!manager_) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorShuttingDown);
  }

  return manager_->CreateWriter(
      isolation_key_, url, last_fetch_time, response_time, expiration, match,
      match_dest, id,
      base::BindOnce(&SharedDictionaryStorageOnDisk::OnDictionaryWritten,
                     weak_factory_.GetWeakPtr(), std::move(matcher)));
}

bool SharedDictionaryStorageOnDisk::UpdateLastFetchTimeIfAlreadyRegistered(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::set<mojom::RequestDestination>& match_dest,
    const std::string& id,
    base::Time last_fetch_time) {
  WrappedDictionaryInfo* matched_info = FindRegisteredInDictionaryInfoMap(
      dictionary_info_map_, url, response_time, expiration, match, match_dest,
      id);
  if (matched_info) {
    manager_->UpdateDictionaryLastFetchTime(*matched_info, last_fetch_time);
    return true;
  }
  return false;
}

void SharedDictionaryStorageOnDisk::OnDatabaseRead(
    net::SQLitePersistentSharedDictionaryStore::DictionaryListOrError result) {
  is_metadata_ready_ = true;

  CHECK(dictionary_info_map_.empty());
  if (!result.has_value()) {
    return;
  }

  for (auto& info : result.value()) {
    const url::SchemeHostPort scheme_host_port =
        url::SchemeHostPort(info.url());
    const std::string match = info.match();
    std::unique_ptr<SimpleUrlPatternMatcher> matcher;
    auto matcher_create_result =
        SimpleUrlPatternMatcher::Create(match, info.url());
    if (!matcher_create_result.has_value()) {
      continue;
    }
    matcher = std::move(matcher_create_result.value());
    WrappedDictionaryInfo wrapped_info(std::move(info), std::move(matcher));
    auto key = std::make_tuple(match, wrapped_info.match_dest());
    dictionary_info_map_[scheme_host_port].insert(
        std::make_pair(std::move(key), std::move(wrapped_info)));
  }

  auto callbacks = std::move(pending_get_dictionary_tasks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void SharedDictionaryStorageOnDisk::OnDictionaryWritten(
    std::unique_ptr<SimpleUrlPatternMatcher> matcher,
    net::SharedDictionaryInfo info) {
  WrappedDictionaryInfo wrapped_info(std::move(info), std::move(matcher));
  const url::SchemeHostPort scheme_host_port =
      url::SchemeHostPort(wrapped_info.url());
  auto key = std::make_tuple(wrapped_info.match(), wrapped_info.match_dest());
  dictionary_info_map_[scheme_host_port].insert_or_assign(
      key, std::move(wrapped_info));
}

void SharedDictionaryStorageOnDisk::OnSharedDictionaryDeleted(
    const base::UnguessableToken& disk_cache_key_token) {
  dictionaries_.erase(disk_cache_key_token);
}

void SharedDictionaryStorageOnDisk::OnDictionaryDeleted(
    const std::set<base::UnguessableToken>& disk_cache_key_tokens) {
  std::erase_if(dictionaries_, [&disk_cache_key_tokens](const auto& it) {
    return disk_cache_key_tokens.find(it.first) != disk_cache_key_tokens.end();
  });

  for (auto& it1 : dictionary_info_map_) {
    std::erase_if(it1.second, [&disk_cache_key_tokens](const auto& it2) {
      return disk_cache_key_tokens.find(it2.second.disk_cache_key_token()) !=
             disk_cache_key_tokens.end();
    });
  }
  std::erase_if(dictionary_info_map_,
                [](const auto& it) { return it.second.empty(); });
}

}  // namespace network
