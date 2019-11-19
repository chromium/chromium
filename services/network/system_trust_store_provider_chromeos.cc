// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/system_trust_store_provider_chromeos.h"

#include <pk11pub.h>

#include <utility>

#include "net/cert/internal/system_trust_store_nss.h"

namespace network {

SystemTrustStoreProviderChromeOS::SystemTrustStoreProviderChromeOS() {}

SystemTrustStoreProviderChromeOS::SystemTrustStoreProviderChromeOS(
    crypto::ScopedPK11Slot user_slot)
    : user_slot_(std::move(user_slot)) {}

SystemTrustStoreProviderChromeOS::~SystemTrustStoreProviderChromeOS() = default;

std::unique_ptr<net::SystemTrustStore>
SystemTrustStoreProviderChromeOS::CreateSystemTrustStore() {
  if (user_slot_) {
    auto user_slot_copy =
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(user_slot_.get()));
    return net::CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
        std::move(user_slot_copy));
  }

  return net::CreateSslSystemTrustStoreNSSWithNoUserSlots();
}

}  // namespace network
