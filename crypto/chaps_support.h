// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CHAPS_SUPPORT_H_
#define CRYPTO_CHAPS_SUPPORT_H_

#include <secmodt.h>

#include "crypto/crypto_export.h"
#include "crypto/scoped_nss_types.h"

namespace crypto {

// Loads chaps module for this NSS session. Should be called on a worker thread.
CRYPTO_EXPORT SECMODModule* LoadChaps();

// Returns a slot with `slot_id` from the `chaps_module`. Should be called on a
// worker thread.
CRYPTO_EXPORT ScopedPK11Slot GetChapsSlot(SECMODModule* chaps_module,
                                          CK_SLOT_ID slot_id);

// Returns true if the given module is the Chaps module. Should be called on a
// worker thread.
CRYPTO_EXPORT bool IsChapsModule(SECMODModule* pk11_module);

// Returns true if chaps is the module to which |slot| is attached. Should be
// called on a worker thread.
CRYPTO_EXPORT bool IsSlotProvidedByChaps(PK11SlotInfo* slot);

}  // namespace crypto

#endif  // CRYPTO_CHAPS_SUPPORT_H_
