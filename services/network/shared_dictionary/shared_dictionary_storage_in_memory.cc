// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_in_memory.h"
#include "url/scheme_host_port.h"

namespace network {

SharedDictionaryStorageInMemory::SharedDictionaryStorageInMemory(
    base::ScopedClosureRunner on_deleted_closure_runner)
    : on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {}

SharedDictionaryStorageInMemory::~SharedDictionaryStorageInMemory() = default;

std::unique_ptr<SharedDictionary>
SharedDictionaryStorageInMemory::GetDictionary(const GURL& url) {
  auto it = origin_to_dictionary_info_map_.find(url::Origin::Create(url));
  if (it == origin_to_dictionary_info_map_.end()) {
    return nullptr;
  }
  const DictionaryInfo* info = nullptr;
  size_t mached_path_size = 0;
  // TODO(crbug.com/1413922): If there are multiple matching dictionaries, this
  // method currently returns the dictionary with the longest path pattern. But
  // we should have a detailed description about `best-matching` in the spec.
  for (const auto& item : it->second) {
    // TODO(crbug.com/1413922): base::MatchPattern() is treating '?' in the
    // pattern as an wildcard. We need to introduce a new flag in
    // base::MatchPattern() to treat '?' as a normal character.
    // TODO(crbug.com/1413922): Need to check the expiration of the dictionary.
    // TODO(crbug.com/1413922): Need support path expansion for relative paths.
    if ((item.first.size() > mached_path_size) &&
        base::MatchPattern(url.path(), item.first)) {
      mached_path_size = item.first.size();
      info = &item.second;
    }
  }

  if (!info) {
    return nullptr;
  }
  return std::make_unique<SharedDictionaryInMemory>(info->data(), info->size(),
                                                    info->hash());
}

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorageInMemory::CreateWriter(const GURL& url,
                                              base::Time response_time,
                                              int64_t expiration,
                                              const std::string& path_pattern) {
  return base::MakeRefCounted<SharedDictionaryWriterInMemory>(
      base::BindOnce(&SharedDictionaryStorageInMemory::OnDictionaryWritten,
                     weak_factory_.GetWeakPtr(), url, response_time, expiration,
                     path_pattern));
}

void SharedDictionaryStorageInMemory::OnDictionaryWritten(
    const GURL& url,
    base::Time response_time,
    int64_t expiration,
    const std::string& path_pattern,
    SharedDictionaryWriterInMemory::Result result,
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& hash) {
  if (result != SharedDictionaryWriterInMemory::Result::kSuccess) {
    return;
  }
  origin_to_dictionary_info_map_[url::Origin::Create(url)].insert(
      std::make_pair(path_pattern,
                     DictionaryInfo(url, response_time, expiration,
                                    path_pattern, data, size, hash)));
}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    const GURL& url,
    base::Time response_time,
    int64_t expiration,
    const std::string& path_pattern,
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& hash)
    : url_(url),
      response_time_(response_time),
      expiration_(expiration),
      path_pattern_(path_pattern),
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
