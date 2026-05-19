// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/mock_unexportable_key.h"

namespace crypto {

MockUnexportableSigningKey::MockUnexportableSigningKey() {
  ON_CALL(*this, AsStatefulKey()).WillByDefault(testing::Return(this));
}
MockUnexportableSigningKey::~MockUnexportableSigningKey() = default;

MockUnexportableAttestationKey::MockUnexportableAttestationKey() {
  ON_CALL(*this, AsStatefulKey()).WillByDefault(testing::Return(this));
}
MockUnexportableAttestationKey::~MockUnexportableAttestationKey() = default;

}  // namespace crypto
