// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_
#define CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_

#include <memory>

#include "crypto/crypto_export.h"

// Forward declaration, from <pk11pub.h>
typedef struct PK11SlotInfoStr PK11SlotInfo;

namespace crypto {

class ScopedTestNSSDB;

// Helper object to override the behavior of `crypto::GetSystemNSSKeySlot()`
// to return a slot from a temporary directory (i.e. bypassing the TPM).
// This object MUST be created before any call to
// `crypto::InitializeTPMTokenAndSystemSlot()`. Note: As noted in
// `crypto::ResetSystemSlotForTesting()`, once a fake slot has been configured
// for a process, it cannot be undone. As such, only one instance of this object
// must be created for a process.
class CRYPTO_EXPORT ScopedTestSystemNSSKeySlot {
 public:
  // If `simulate_token_loader` is false, this class only prepares a software
  // system slot, which will be made available through `GetSystemNSSKeySlot`
  // when something else (presumably the TpmTokenLoader) calls
  // `crypto::FinishInitializingTPMTokenAndSystemSlot`. Setting
  // `simulate_token_loader` to true emulates the "initialization finished"
  // signal immediately (e.g. in unit tests).
  ScopedTestSystemNSSKeySlot(bool simulate_token_loader);

  ScopedTestSystemNSSKeySlot(const ScopedTestSystemNSSKeySlot&) = delete;
  ScopedTestSystemNSSKeySlot& operator=(const ScopedTestSystemNSSKeySlot&) =
      delete;

  ~ScopedTestSystemNSSKeySlot();

  bool ConstructedSuccessfully() const;
  PK11SlotInfo* slot() const;

 private:
  std::unique_ptr<ScopedTestNSSDB> test_db_;
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_
