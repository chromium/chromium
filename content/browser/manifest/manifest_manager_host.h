// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
#define CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_

#include <optional>

#include "base/containers/id_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/page_manifest_manager.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_observer.mojom.h"
#include "url/gurl.h"

namespace content {

// ManifestManagerHost is a helper class that allows callers to get the Manifest
// associated with the main frame of the observed WebContents. It handles the
// IPC messaging with the child process.
// TODO(mlamouri): keep a cached version and a dirty bit here.
class CONTENT_EXPORT ManifestManagerHost
    : public PageUserData<ManifestManagerHost>,
      public PageManifestManager,
      public blink::mojom::ManifestUrlChangeObserver {
 public:
  ManifestManagerHost(const ManifestManagerHost&) = delete;
  ManifestManagerHost& operator=(const ManifestManagerHost&) = delete;

  ~ManifestManagerHost() override;

  using GetManifestCallback =
      base::OnceCallback<void(blink::mojom::ManifestRequestResult,
                              const GURL&,
                              blink::mojom::ManifestPtr)>;

  // Calls the given callback with the manifest associated with the main frame.
  // If the main frame has no manifest or if getting it failed the callback will
  // have an empty manifest.
  void GetManifest(GetManifestCallback callback);

  base::CallbackListSubscription GetSpecifiedManifest(
      ManifestCallbackList::CallbackType callback) override;
  base::CallbackListSubscription GetAllSpecifiedManifests(
      AllManifestsCallbackList::CallbackType callback) override;

  void RequestManifestDebugInfo(
      blink::mojom::ManifestManager::RequestManifestDebugInfoCallback callback);

  void BindObserver(
      mojo::PendingAssociatedReceiver<blink::mojom::ManifestUrlChangeObserver>
          receiver);

  // Exposes internal manifest validation and override logic for tests. This
  // allows testing the bad message termination (e.g., cross-site migration
  // checks) natively without spinning up an end-to-end generic mojo pipe.
  // Note: Callers testing mojo::ReportBadMessage usually need to setup a
  // mojo::FakeMessageDispatchContext before calling this method.
  void ValidateAndMaybeOverrideManifestForTesting(
      blink::mojom::ManifestRequestResult result,
      blink::mojom::ManifestPtr manifest);

 private:
  explicit ManifestManagerHost(Page& page);

  friend class PageUserData<ManifestManagerHost>;

  using CallbackMap = base::IDMap<std::unique_ptr<GetManifestCallback>>;

  blink::mojom::ManifestManager& GetManifestManager();

  // If it is a bad message, returns an unpopulated Manifest instance.
  blink::mojom::ManifestPtr ValidateAndMaybeOverrideManifest(
      blink::mojom::ManifestRequestResult result,
      blink::mojom::ManifestPtr manifest);

  std::vector<GetManifestCallback> ExtractPendingCallbacks();
  void OnConnectionError();

  void OnRequestManifestResponse(int request_id,
                                 blink::mojom::ManifestRequestResult result,
                                 const GURL& url,
                                 blink::mojom::ManifestPtr manifest);
  void OnRequestManifestAndErrors(
      const GURL& manifest_url_for_fetch,
      base::expected<blink::mojom::ManifestPtr,
                     blink::mojom::RequestManifestErrorPtr>);

  // blink::mojom::ManifestUrlChangeObserver:
  void ManifestUrlChanged(const GURL& manifest_url) override;

  void MaybeFetchManifestForSubscriptions();

  void NotifyOnceSubscriptionsIfSuccessCached();

  void NotifySubscriptionsIfSuccessCached();

  bool HasManifestSubscriptions() const;

  PAGE_USER_DATA_KEY_DECL();

  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;
  CallbackMap callbacks_;

  std::optional<blink::mojom::ManifestPtr> last_manifest_success_result_ =
      std::nullopt;

  // Keep track of the manifest url that is currently being fetched, to prevent
  // duplicate manifest fetches being triggered if one is already in progress.
  std::optional<GURL> current_fetching_manifest_url_;

  ManifestCallbackList developer_manifest_callback_list_;
  AllManifestsCallbackList all_manifests_callback_list_;

  mojo::AssociatedReceiver<blink::mojom::ManifestUrlChangeObserver>
      manifest_url_change_observer_receiver_{this};

  base::WeakPtrFactory<ManifestManagerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
