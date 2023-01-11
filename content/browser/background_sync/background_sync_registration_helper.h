// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HELPER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HELPER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/public/browser/background_sync_registration.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BackgroundSyncContextImpl;
class RenderProcessHost;

// Used by OneShotBackgroundSyncService and PeriodicBackgroundSyncService to
// create and get BackgroundSync registrations.
class BackgroundSyncRegistrationHelper {
 public:
  using RegisterCallback =
      base::OnceCallback<void(blink::mojom::BackgroundSyncError status,
                              blink::mojom::SyncRegistrationOptionsPtr result)>;
  using GetRegistrationsCallback = base::OnceCallback<void(
      blink::mojom::BackgroundSyncError status,
      std::vector<blink::mojom::SyncRegistrationOptionsPtr> results)>;

  BackgroundSyncRegistrationHelper(
      BackgroundSyncContextImpl* background_sync_context,
      RenderProcessHost* render_process_host);

  BackgroundSyncRegistrationHelper(const BackgroundSyncRegistrationHelper&) =
      delete;
  BackgroundSyncRegistrationHelper& operator=(
      const BackgroundSyncRegistrationHelper&) = delete;

  ~BackgroundSyncRegistrationHelper();

  bool ValidateSWRegistrationID(int64_t sw_registration_id,
                                const url::Origin& origin);

  void Register(blink::mojom::SyncRegistrationOptionsPtr options,
                int64_t sw_registration_id,
                RegisterCallback callback);
  void DidResolveRegistration(
      blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info);
  void OnRegisterResult(RegisterCallback callback,
                        BackgroundSyncStatus status,
                        std::unique_ptr<BackgroundSyncRegistration> result);
  void NotifyInvalidOptionsProvided(RegisterCallback callback) const;
  void OnGetRegistrationsResult(
      GetRegistrationsCallback callback,
      BackgroundSyncStatus status,
      std::vector<std::unique_ptr<BackgroundSyncRegistration>>
          result_registrations);

  base::WeakPtr<BackgroundSyncRegistrationHelper> GetWeakPtr();

 private:
  // |background_sync_context_| (indirectly) owns |this|.
  const raw_ptr<BackgroundSyncContextImpl> background_sync_context_;
  int render_process_host_id_;
  base::WeakPtrFactory<BackgroundSyncRegistrationHelper> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HELPER_H_
