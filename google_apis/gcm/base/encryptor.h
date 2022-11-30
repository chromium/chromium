// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_ENCRYPTOR_H_
#define GOOGLE_APIS_GCM_BASE_ENCRYPTOR_H_

#include <string>
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {

class GCM_EXPORT Encryptor {
 public:
  // All methods below should be thread-safe.
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) = 0;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) = 0;

  virtual ~Encryptor() {}
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_ENCRYPTOR_H_
