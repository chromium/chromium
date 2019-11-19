// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_
#define NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_

#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/system_trust_store.h"

namespace net {

// Create a SystemTrustStore that will accept trust for:
// (*) built-in certificates
// (*) test root certificates
// (*) additional trust anchors (added through SystemTrustStore::AddTrustAnchor)
// (*) certificates stored on the |user_slot|.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
    crypto::ScopedPK11Slot user_slot);

// Create a SystemTrustStore that will accept trust for:
// (*) built-in certificates
// (*) test root certificates
// (*) additional trust anchors (added through SystemTrustStore::AddTrustAnchor)
// It will not accept trust for certificates stored on other slots.
NET_EXPORT std::unique_ptr<SystemTrustStore>
CreateSslSystemTrustStoreNSSWithNoUserSlots();

}  // namespace net

#endif  // NET_CERT_INTERNAL_SYSTEM_TRUST_STORE_NSS_H_
