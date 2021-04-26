// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
#define CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_

#include "base/callback_forward.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_observer.mojom.h"

namespace blink {
struct Manifest;
}

namespace content {

class RenderFrameHostImpl;

// ManifestManagerHost is a helper class that allows callers to get the Manifest
// associated with the main frame of the observed WebContents. It handles the
// IPC messaging with the child process.
// TODO(mlamouri): keep a cached version and a dirty bit here.
class ManifestManagerHost
    : public RenderDocumentHostUserData<ManifestManagerHost>,
      public blink::mojom::ManifestUrlChangeObserver {
 public:
  ~ManifestManagerHost() override;

  using GetManifestCallback =
      base::OnceCallback<void(const GURL&, const blink::Manifest&)>;

  // Calls the given callback with the manifest associated with the main frame.
  // If the main frame has no manifest or if getting it failed the callback will
  // have an empty manifest.
  void GetManifest(GetManifestCallback callback);

  void RequestManifestDebugInfo(
      blink::mojom::ManifestManager::RequestManifestDebugInfoCallback callback);

  void BindObserver(
      mojo::PendingAssociatedReceiver<blink::mojom::ManifestUrlChangeObserver>
          receiver);

 private:
  explicit ManifestManagerHost(RenderFrameHost* render_frame_host);

  friend class RenderDocumentHostUserData<ManifestManagerHost>;

  using CallbackMap = base::IDMap<std::unique_ptr<GetManifestCallback>>;

  blink::mojom::ManifestManager& GetManifestManager();

  void DispatchPendingCallbacks();
  void OnConnectionError();

  void OnRequestManifestResponse(int request_id,
                                 const GURL& url,
                                 const blink::Manifest& manifest);

  // blink::mojom::ManifestUrlChangeObserver:
  void ManifestUrlChanged(const base::Optional<GURL>& manifest_url) override;

  RenderFrameHostImpl* manifest_manager_frame_;
  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;
  CallbackMap callbacks_;

  mojo::AssociatedReceiver<blink::mojom::ManifestUrlChangeObserver>
      manifest_url_change_observer_receiver_{this};

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(ManifestManagerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
