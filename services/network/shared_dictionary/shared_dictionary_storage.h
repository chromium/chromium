// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_

#include <set>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

class SharedDictionary;
class SharedDictionaryWriter;

// Shared Dictionary Storage manages dictionaries for a particular
// net::NetworkIsolationKey.
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
      int64_t expiration,
      const std::string& path_pattern) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_H_
