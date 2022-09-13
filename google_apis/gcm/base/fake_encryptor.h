// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_FAKE_ENCRYPTOR_H_
#define GOOGLE_APIS_GCM_BASE_FAKE_ENCRYPTOR_H_

#include "base/compiler_specific.h"
#include "google_apis/gcm/base/encryptor.h"

namespace gcm {

// Encryptor which simply base64-encodes the plaintext to get the
// ciphertext.  Obviously, this should be used only for testing.
class FakeEncryptor : public Encryptor {
 public:
  ~FakeEncryptor() override;

  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;

  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_FAKE_ENCRYPTOR_H_
