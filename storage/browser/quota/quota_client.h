// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

// Interface between the legacy quota clients and the QuotaManager.
//
// Implementations of this class will be transitioned to inherit from
// storage::mojom::QuotaClient and talk to the QuotaManager via mojo.
//
// This inherits from storage::mojom::QuotaClient so that MockQuotaClient
// instances can be passed to QuotaManger::RegisterLegacyClient(),
// as well as used via mojo with QuotaManager::RegisterClient().
//
// TODO(crbug.com/1163009): Remove this class after all QuotaClients have
//                          been mojofied.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaClient
    : public base::RefCountedThreadSafe<QuotaClient>,
      public storage::mojom::QuotaClient {
 public:
  // Called when the QuotaManager is destroyed.
  virtual void OnQuotaManagerDestroyed() = 0;

 protected:
  friend class RefCountedThreadSafe<QuotaClient>;

  ~QuotaClient() override = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_H_
