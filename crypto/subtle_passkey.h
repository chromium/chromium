// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SUBTLE_PASSKEY_H_
#define CRYPTO_SUBTLE_PASSKEY_H_

#include "crypto/crypto_export.h"

namespace syncer {
class Nigori;
}

namespace crypto {

// A crypto::SubtlePassKey allows you to call subtle, difficult-to-get-right, or
// mistake-prone APIs, or APIs that allow you to make detailed cryptographic
// choices for yourself. See //docs/patterns/passkey.md for details.
//
// Note: this has no relation at all to the "passkey" WebAuthN mechanism.
class CRYPTO_EXPORT SubtlePassKey final {
 public:
  ~SubtlePassKey();

  // Test code is always allowed to use these APIs.
  static SubtlePassKey ForTesting();

 private:
  SubtlePassKey();

  // Deprecated: remove this once the DeriveKey*() methods are deleted from
  // SymmetricKey.
  friend class SymmetricKey;

  // This class uses custom PBKDF2 parameters - the Nigori spec requires this.
  friend class syncer::Nigori;
};

}  // namespace crypto

#endif  // CRYPTO_SUBTLE_PASSKEY_H_
