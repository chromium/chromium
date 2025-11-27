// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_
#define NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT CacheEncryptionDelegate {
 public:
  CacheEncryptionDelegate() = default;

  virtual ~CacheEncryptionDelegate() = default;

  // Async init. Don't call any other methods before |callback| is called. The
  // callback will be run with net::OK on success, or an error code on failure.
  // If already initialized, should run the callback immediately.
  virtual void Init(base::OnceCallback<void(Error)> callback) = 0;

  virtual bool EncryptData(base::span<const uint8_t> plaintext,
                           std::vector<uint8_t>* ciphertext) = 0;
  virtual bool DecryptData(base::span<const uint8_t> ciphertext,
                           std::vector<uint8_t>* plaintext) = 0;
};

}  // namespace net

#endif  // NET_DISK_CACHE_CACHE_ENCRYPTION_DELEGATE_H_
