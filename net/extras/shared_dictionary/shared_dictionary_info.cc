// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/shared_dictionary/shared_dictionary_info.h"

namespace net {

SharedDictionaryInfo::SharedDictionaryInfo(
    const GURL& url,
    base::Time last_fetch_time,
    base::Time response_time,
    base::TimeDelta expiration,
    const std::string& match,
    const std::string& match_dest_string,
    const std::string& id,
    base::Time last_used_time,
    size_t size,
    const net::SHA256HashValue& hash,
    const base::UnguessableToken& disk_cache_key_token,
    const std::optional<int64_t>& primary_key_in_database)
    : url_(url),
      last_fetch_time_(last_fetch_time),
      response_time_(response_time),
      expiration_(expiration),
      match_(match),
      match_dest_string_(match_dest_string),
      id_(id),
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

bool SharedDictionaryInfo::operator==(const SharedDictionaryInfo& other) const =
    default;

base::Time SharedDictionaryInfo::GetExpirationTime() const {
  return response_time_ + expiration_;
}

}  // namespace net
