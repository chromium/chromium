// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_test_system_nss_key_slot.h"

#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_db.h"

namespace crypto {

ScopedTestSystemNSSKeySlot::ScopedTestSystemNSSKeySlot(
    bool simulate_token_loader)
    : test_db_(new ScopedTestNSSDB) {
  if (!test_db_->is_open())
    return;

  PrepareSystemSlotForTesting(  // IN-TEST
      ScopedPK11Slot(PK11_ReferenceSlot(test_db_->slot())));

  if (simulate_token_loader)
    FinishInitializingTPMTokenAndSystemSlot();
}

ScopedTestSystemNSSKeySlot::~ScopedTestSystemNSSKeySlot() {
  ResetSystemSlotForTesting();  // IN-TEST
}

bool ScopedTestSystemNSSKeySlot::ConstructedSuccessfully() const {
  return test_db_->is_open();
}

PK11SlotInfo* ScopedTestSystemNSSKeySlot::slot() const {
  return test_db_->slot();
}

}  // namespace crypto
