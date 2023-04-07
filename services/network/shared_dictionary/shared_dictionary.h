// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace net {
class IOBuffer;
struct SHA256HashValue;
}  // namespace net

namespace network {

// This class is used to read the binary of the shared dictionary.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionary {
 public:
  virtual ~SharedDictionary() = default;

  // Reads the whole binary of the dictionary. If an error has occurred, returns
  // net::ERR_FAILED. If the binary of the dictionary is already in the memory
  // returns net::OK. Otherwise returns net::ERR_IO_PENDING and `callback` will
  // be called asynchronously with net::OK or net::ERR_FAILED depending on the
  // success status.
  virtual int ReadAll(base::OnceCallback<void(int)> callback) = 0;

  // Returns the buffer which contains the binary of the dictionary.
  // ReadAll() must have succeeded before calling this method.
  virtual scoped_refptr<net::IOBuffer> data() const = 0;

  // Returns the binary size of the dictionary. It is safe to call this method
  // before calling ReadAll().
  virtual size_t size() const = 0;

  // Returns the hash of the binary of the dictionary. It is safe to call this
  // method before calling ReadAll().
  virtual const net::SHA256HashValue& hash() const = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_H_
