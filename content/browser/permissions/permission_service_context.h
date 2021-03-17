// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_

#include <memory>
#include <unordered_map>

#include "content/common/content_export.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;
class RenderProcessHost;

// Provides information to a PermissionService. It is used by the
// PermissionServiceImpl to handle request permission UI.
// There is one PermissionServiceContext per RenderFrameHost/RenderProcessHost
// which owns it. It then owns all PermissionServiceImpl associated to their
// owner.
class CONTENT_EXPORT PermissionServiceContext : public WebContentsObserver {
 public:
  explicit PermissionServiceContext(RenderFrameHost* render_frame_host);
  explicit PermissionServiceContext(RenderProcessHost* render_process_host);
  PermissionServiceContext(const PermissionServiceContext&) = delete;
  PermissionServiceContext& operator=(const PermissionServiceContext&) = delete;
  ~PermissionServiceContext() override;

  void CreateService(
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver);
  void CreateServiceForWorker(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::PermissionService> receiver);

  void CreateSubscription(
      PermissionType permission_type,
      const url::Origin& origin,
      blink::mojom::PermissionStatus current_status,
      blink::mojom::PermissionStatus last_known_status,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer);

  // Called when the connection to a PermissionObserver has an error.
  void ObserverHadConnectionError(int subscription_id);

  // May return nullptr during teardown, or when showing an interstitial.
  BrowserContext* GetBrowserContext() const;

  GURL GetEmbeddingOrigin() const;

  RenderFrameHost* render_frame_host() const { return render_frame_host_; }
  RenderProcessHost* render_process_host() const {
    return render_process_host_;
  }

 private:
  class PermissionSubscription;

  // WebContentsObserver
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void CloseBindings(RenderFrameHost* render_frame_host);

  RenderFrameHost* const render_frame_host_;
  RenderProcessHost* const render_process_host_;
  mojo::UniqueReceiverSet<blink::mojom::PermissionService> services_;
  std::unordered_map<int, std::unique_ptr<PermissionSubscription>>
      subscriptions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SERVICE_CONTEXT_H_
