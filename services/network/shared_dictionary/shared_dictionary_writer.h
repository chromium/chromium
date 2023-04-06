// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace network {

// SharedDictionaryWriter is used to write a dictionary. After Finish() is
// called the dictionary is stored to the storage.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryWriter
    : public base::RefCounted<SharedDictionaryWriter> {
 public:
  // Appends the binary to the dictionary.
  virtual void Append(const char* buf, int num_bytes) = 0;

  // Finishes writing to the dictionary.
  // Note: Currently there is no implementation of SharedDictionaryWriter which
  // stores the dictionary to the disk. But when we implement it, `this` will be
  // kept alive until the disk operation finishes.
  virtual void Finish() = 0;

 protected:
  friend class base::RefCounted<SharedDictionaryWriter>;
  virtual ~SharedDictionaryWriter() = default;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_WRITER_H_
