// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SYSTEM_TRUST_STORE_PROVIDER_CHROMEOS_H_
#define SERVICES_NETWORK_SYSTEM_TRUST_STORE_PROVIDER_CHROMEOS_H_

#include <certt.h>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/nss_profile_filter_chromeos.h"

namespace net {
class SystemTrustStore;
}

namespace network {

// A SystemTrustStoreProvider that supports creating SystemTrustStore instances
// which will only consider user-imported certificates trusted if they are on a
// specific NSS slot.
class COMPONENT_EXPORT(NETWORK_SERVICE) SystemTrustStoreProviderChromeOS
    : public net::SystemTrustStoreProvider {
 public:
  // Creates a SystemTrustStoreProvider that will provide SystemTrustStore
  // instances which will not allow trusting user-imported certififcates.
  SystemTrustStoreProviderChromeOS();

  // Creates a SystemTrustStoreProvider that will provide SystemTrustStore
  // instances which will only consider user-imported certificates trusted if
  // they are on |user_slot|.
  explicit SystemTrustStoreProviderChromeOS(crypto::ScopedPK11Slot user_slot);

  ~SystemTrustStoreProviderChromeOS() override;

  // As SystemTrustStoreProvider states, this must be thread-safe and will be
  // called concurrently from worker threads.
  std::unique_ptr<net::SystemTrustStore> CreateSystemTrustStore() override;

 private:
  const crypto::ScopedPK11Slot user_slot_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrustStoreProviderChromeOS);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SYSTEM_TRUST_STORE_PROVIDER_CHROMEOS_H_
