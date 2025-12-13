// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_SECURITY_FRAMEWORK_LOCK_H_
#define CRYPTO_APPLE_SECURITY_FRAMEWORK_LOCK_H_

#include "crypto/crypto_export.h"

namespace base {
class Lock;
}

namespace crypto::apple {

// Some of the APIs exported by macOS Security.framework cannot be called
// concurrently. This lock protects calls to:
//   SecKeychain*()
//   SecPolicy*()
//   SecTrust*()
// See
// https://developer.apple.com/documentation/security/certificate_key_and_trust_services/working_with_concurrency
// for more details.
CRYPTO_EXPORT base::Lock& GetSecurityFrameworkLock();

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_SECURITY_FRAMEWORK_LOCK_H_
