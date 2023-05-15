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
  const DictionaryInfo* info =
      GetMatchingDictionaryFromDictionaryInfoMap(dictionary_info_map_, url);

  if (!info) {
    return nullptr;
  }
  return std::make_unique<SharedDictionaryInMemory>(info->data(), info->size(),
                                                    info->hash());
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
      DictionaryInfo(url, response_time, expiration, match, data, size, hash)));
}

SharedDictionaryStorageInMemory::DictionaryInfo::DictionaryInfo(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& hash)
    : url_(url),
      response_time_(response_time),
      expiration_(expiration),
      match_(match),
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
