// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CHAPS_SUPPORT_H_
#define CRYPTO_CHAPS_SUPPORT_H_

#include <secmodt.h>

#include "crypto/crypto_export.h"

namespace crypto {

// Loads chaps module for this NSS session.
CRYPTO_EXPORT SECMODModule* LoadChaps();

// Returns true if chaps is the module to which |slot| is attached.
CRYPTO_EXPORT bool IsSlotProvidedByChaps(PK11SlotInfo* slot);

}  // namespace crypto

#endif  // CRYPTO_CHAPS_SUPPORT_H_
