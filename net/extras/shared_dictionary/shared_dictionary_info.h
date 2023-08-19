// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_INFO_H_
#define NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_INFO_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "net/base/hash_value.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

// This class represents a shared dictionary information stored in a SQLite
// database for compression dictionary transport feature.
class COMPONENT_EXPORT(NET_SHARED_DICTIONARY) SharedDictionaryInfo {
 public:
  SharedDictionaryInfo(const GURL& url,
                       base::Time response_time,
                       base::TimeDelta expiration,
                       const std::string& match,
                       base::Time last_used_time,
                       size_t size,
                       const net::SHA256HashValue& hash,
                       const base::UnguessableToken& disk_cache_key_token,
                       const absl::optional<int64_t>& primary_key_in_database);

  SharedDictionaryInfo(const SharedDictionaryInfo&);
  SharedDictionaryInfo& operator=(const SharedDictionaryInfo&);

  SharedDictionaryInfo(SharedDictionaryInfo&& other);
  SharedDictionaryInfo& operator=(SharedDictionaryInfo&& other);

  ~SharedDictionaryInfo();

  bool operator==(const SharedDictionaryInfo& other) const;

  const GURL& url() const { return url_; }
  base::Time response_time() const { return response_time_; }
  base::TimeDelta expiration() const { return expiration_; }
  const std::string& match() const { return match_; }
  base::Time last_used_time() const { return last_used_time_; }
  size_t size() const { return size_; }
  const net::SHA256HashValue& hash() const { return hash_; }
  const base::UnguessableToken& disk_cache_key_token() const {
    return disk_cache_key_token_;
  }

  const absl::optional<int64_t>& primary_key_in_database() const {
    return primary_key_in_database_;
  }
  void set_primary_key_in_database(int64_t primary_key_in_database) {
    primary_key_in_database_ = primary_key_in_database;
  }

  void set_last_used_time(base::Time last_used_time) {
    last_used_time_ = last_used_time;
  }

  // An utility method that returns `response_time_` + `expiration_`.
  base::Time GetExpirationTime() const;

 private:
  // URL of the dictionary.
  GURL url_;
  // The time of when the dictionary was received from the server.
  base::Time response_time_;
  // The expiration time for the dictionary which was declared in
  // 'use-as-dictionary' response header's `expires` option in seconds.
  base::TimeDelta expiration_;
  // The matching path pattern for the dictionary which was declared in
  // 'use-as-dictionary' response header's `match` option.
  std::string match_;
  // The time when the dictionary was last used.
  base::Time last_used_time_;
  // The size of the dictionary binary.
  size_t size_;
  // The sha256 hash of the dictionary binary.
  net::SHA256HashValue hash_;
  // The UnguessableToken used as a key in the DiskCache to store the dictionary
  // binary.
  base::UnguessableToken disk_cache_key_token_;
  // The primary key in SQLite database. This is nullopt until it is stored to
  // the database.
  absl::optional<int64_t> primary_key_in_database_;
};

}  // namespace net

#endif  // NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_INFO_H_
