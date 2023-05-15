// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/shared_dictionary/shared_dictionary_info.h"

namespace net {

SharedDictionaryInfo::SharedDictionaryInfo(
    const GURL& url,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    base::Time last_used_time,
    size_t size,
    const net::SHA256HashValue& hash,
    const base::UnguessableToken& disk_cache_key_token,
    const absl::optional<int64_t>& primary_key_in_database)
    : url_(url),
      response_time_(response_time),
      expiration_(expiration),
      match_(match),
      last_used_time_(last_used_time),
      size_(size),
      hash_(hash),
      disk_cache_key_token_(disk_cache_key_token),
      primary_key_in_database_(primary_key_in_database) {}

SharedDictionaryInfo::SharedDictionaryInfo(const SharedDictionaryInfo&) =
    default;
SharedDictionaryInfo& SharedDictionaryInfo::operator=(
    const SharedDictionaryInfo&) = default;

SharedDictionaryInfo::SharedDictionaryInfo(SharedDictionaryInfo&&) = default;
SharedDictionaryInfo& SharedDictionaryInfo::operator=(SharedDictionaryInfo&&) =
    default;

SharedDictionaryInfo::~SharedDictionaryInfo() = default;

bool SharedDictionaryInfo::operator==(const SharedDictionaryInfo& other) const {
  return url_ == other.url_ && response_time_ == other.response_time_ &&
         expiration_ == other.expiration_ && match_ == other.match_ &&
         last_used_time_ == other.last_used_time_ && size_ == other.size_ &&
         hash_ == other.hash_ &&
         disk_cache_key_token_ == other.disk_cache_key_token_ &&
         primary_key_in_database_ == other.primary_key_in_database_;
}

base::Time SharedDictionaryInfo::GetExpirationTime() const {
  return response_time_ + expiration_;
}

}  // namespace net
