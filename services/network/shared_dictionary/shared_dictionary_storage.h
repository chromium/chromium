// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_

#include <map>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

class SharedDictionary;
class SharedDictionaryWriter;

// Shared Dictionary Storage manages dictionaries for a particular
// net::SharedDictionaryStorageIsolationKey.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryStorage
    : public base::RefCounted<SharedDictionaryStorage> {
 public:
  SharedDictionaryStorage(const SharedDictionaryStorage&) = delete;
  SharedDictionaryStorage& operator=(const SharedDictionaryStorage&) = delete;

  // Returns a matching SharedDictionary for `url`.
  virtual std::unique_ptr<SharedDictionary> GetDictionary(const GURL& url) = 0;

  // Returns a SharedDictionaryWriter if `headers` has a valid
  // `use-as-dictionary` header.
  scoped_refptr<SharedDictionaryWriter> MaybeCreateWriter(
      const GURL& url,
      base::Time response_time,
      const net::HttpResponseHeaders& headers);

 protected:
  friend class base::RefCounted<SharedDictionaryStorage>;

  SharedDictionaryStorage();
  virtual ~SharedDictionaryStorage();

  // Called to create a SharedDictionaryWriter.
  virtual scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match) = 0;
};

// Returns a matching dictionary for `url` from `dictionary_info_map`.
// This is a template method because SharedDictionaryStorageOnDisk and
// SharedDictionaryStorageOnDisk are using different class for
// DictionaryInfoType.
template <class DictionaryInfoType>
const DictionaryInfoType* GetMatchingDictionaryFromDictionaryInfoMap(
    const std::map<url::SchemeHostPort,
                   std::map<std::string, DictionaryInfoType>>&
        dictionary_info_map,
    const GURL& url) {
  auto it = dictionary_info_map.find(url::SchemeHostPort(url));
  if (it == dictionary_info_map.end()) {
    return nullptr;
  }
  const DictionaryInfoType* info = nullptr;
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
  return info;
}

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_
