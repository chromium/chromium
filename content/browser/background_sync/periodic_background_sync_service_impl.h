// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_registration_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class BackgroundSyncContextImpl;

class CONTENT_EXPORT PeriodicBackgroundSyncServiceImpl
    : public blink::mojom::PeriodicBackgroundSyncService {
 public:
  PeriodicBackgroundSyncServiceImpl(
      BackgroundSyncContextImpl* background_sync_context,
      mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
          receiver);

  ~PeriodicBackgroundSyncServiceImpl() override;

 private:
  friend class PeriodicBackgroundSyncServiceImplTest;

  // blink::mojom::BackgroundSyncService methods:
  void Register(blink::mojom::SyncRegistrationOptionsPtr options,
                int64_t sw_registration_id,
                RegisterCallback callback) override;
  void Unregister(int64_t sw_registration_id,
                  const std::string& tag,
                  UnregisterCallback callback) override;
  void GetRegistrations(int64_t sw_registration_id,
                        GetRegistrationsCallback callback) override;

  void OnUnregisterResult(UnregisterCallback callback,
                          BackgroundSyncStatus status);

  // Called when a disconnection is detected on |receiver_|.
  void OnMojoDisconnect();

  // |background_sync_context_| owns |this|.
  BackgroundSyncContextImpl* background_sync_context_;

  std::unique_ptr<BackgroundSyncRegistrationHelper> registration_helper_;
  mojo::Receiver<blink::mojom::PeriodicBackgroundSyncService> receiver_;

  base::WeakPtrFactory<PeriodicBackgroundSyncServiceImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_PERIODIC_BACKGROUND_SYNC_SERVICE_IMPL_H_
