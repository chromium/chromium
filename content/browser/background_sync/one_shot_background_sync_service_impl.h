// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_ONE_SHOT_BACKGROUND_SYNC_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_ONE_SHOT_BACKGROUND_SYNC_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_registration_helper.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "url/origin.h"

namespace content {

class BackgroundSyncContextImpl;

class CONTENT_EXPORT OneShotBackgroundSyncServiceImpl
    : public blink::mojom::OneShotBackgroundSyncService {
 public:
  OneShotBackgroundSyncServiceImpl(
      BackgroundSyncContextImpl* background_sync_context,
      const url::Origin& origin,
      RenderProcessHost* render_process_host,
      mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
          receiver);

  OneShotBackgroundSyncServiceImpl(const OneShotBackgroundSyncServiceImpl&) =
      delete;
  OneShotBackgroundSyncServiceImpl& operator=(
      const OneShotBackgroundSyncServiceImpl&) = delete;

  ~OneShotBackgroundSyncServiceImpl() override;

 private:
  friend class OneShotBackgroundSyncServiceImplTest;

  // blink::mojom::OneShotBackgroundSyncService methods:
  void Register(blink::mojom::SyncRegistrationOptionsPtr options,
                int64_t sw_registration_id,
                RegisterCallback callback) override;
  void DidResolveRegistration(blink::mojom::BackgroundSyncRegistrationInfoPtr
                                  registration_info) override;
  void GetRegistrations(int64_t sw_registration_id,
                        GetRegistrationsCallback callback) override;

  // Called when a disconnection is detected on |receiver_|.
  void OnMojoDisconnect();

  // |background_sync_context_| owns |this|.
  const raw_ptr<BackgroundSyncContextImpl> background_sync_context_;

  url::Origin origin_;

  std::unique_ptr<BackgroundSyncRegistrationHelper> registration_helper_;
  mojo::Receiver<blink::mojom::OneShotBackgroundSyncService> receiver_;

  base::WeakPtrFactory<blink::mojom::OneShotBackgroundSyncService>
      weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_ONE_SHOT_BACKGROUND_SYNC_SERVICE_IMPL_H_
