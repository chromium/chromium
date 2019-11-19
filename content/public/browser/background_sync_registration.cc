// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_sync_registration.h"

namespace content {

BackgroundSyncRegistration::BackgroundSyncRegistration() = default;
BackgroundSyncRegistration::BackgroundSyncRegistration(
    const BackgroundSyncRegistration& other) = default;
BackgroundSyncRegistration& BackgroundSyncRegistration::operator=(
    const BackgroundSyncRegistration& other) = default;
BackgroundSyncRegistration::~BackgroundSyncRegistration() = default;

bool BackgroundSyncRegistration::Equals(
    const BackgroundSyncRegistration& other) const {
  return options_.Equals(other.options_);
}

bool BackgroundSyncRegistration::IsFiring() const {
  switch (sync_state_) {
    case blink::mojom::BackgroundSyncState::FIRING:
    case blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING:
      return true;
    case blink::mojom::BackgroundSyncState::PENDING:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
