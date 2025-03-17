// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SUBTLE_PASSKEY_H_
#define CRYPTO_SUBTLE_PASSKEY_H_

#include "crypto/crypto_export.h"

namespace ash {
class CryptohomeTokenEncryptor;
class Key;
}

namespace syncer {
class Nigori;
}

namespace crypto {
class SubtlePassKey;
}  // namespace crypto

namespace chromeos::onc {
crypto::SubtlePassKey MakeCryptoPassKey();
}

namespace os_crypt_async {
class FreedesktopSecretKeyProvider;
}

class OSCryptImpl;

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

  // This class uses custom PBKDF2 parameters, and has to keep doing so for
  // compatibility with persisted data on disk.
  friend class ash::CryptohomeTokenEncryptor;

  // This class uses custom PBKDF2 parameters - the Nigori spec requires this.
  friend class syncer::Nigori;

  // ONC EncryptedConfiguration objects can contain and require us to use
  // arbitrary (possibly attacker-supplied) PBKDF2 parameters.
  friend SubtlePassKey chromeos::onc::MakeCryptoPassKey();

  // These classes use custom PBKDF2 parameters and have to keep doing so for
  // compatibility with existing persisted data.
  friend class ::OSCryptImpl;
  friend class os_crypt_async::FreedesktopSecretKeyProvider;

  // This class uses custom PBKDF2 parameters which cannot be changed for
  // compatibility with persisted data.
  friend class ash::Key;
};

}  // namespace crypto

#endif  // CRYPTO_SUBTLE_PASSKEY_H_
