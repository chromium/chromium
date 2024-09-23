// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace net {
class IOBuffer;
struct SHA256HashValue;

// This class is used to read the binary of the shared dictionary.
class NET_EXPORT SharedDictionary : public base::RefCounted<SharedDictionary> {
 public:
  // Reads the whole binary of the dictionary. If an error has occurred, returns
  // ERR_FAILED. If the binary of the dictionary is already in the memory
  // returns OK. Otherwise returns ERR_IO_PENDING and `callback` will be called
  // asynchronously with OK or ERR_FAILED depending on the success status.
  virtual int ReadAll(base::OnceCallback<void(int)> callback) = 0;

  // Returns the buffer which contains the binary of the dictionary.
  // ReadAll() must have succeeded before calling this method.
  virtual scoped_refptr<IOBuffer> data() const = 0;

  // Returns the binary size of the dictionary. It is safe to call this method
  // before calling ReadAll().
  virtual size_t size() const = 0;

  // Returns the hash of the binary of the dictionary. It is safe to call this
  // method before calling ReadAll().
  virtual const SHA256HashValue& hash() const = 0;

  // Returns the server-provided id of the dictionary. When this id is not
  // empty, it will be serialized [RFC8941] and sent in "Dictionary-ID" request
  // when Chrome can use the dictionary.
  // https://www.rfc-editor.org/rfc/rfc8941#name-serializing-a-string
  virtual const std::string& id() const = 0;

 protected:
  friend class base::RefCounted<SharedDictionary>;
  virtual ~SharedDictionary() = default;
};

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_H_
