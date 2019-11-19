// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_H_

#include <memory>
#include <string>

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"

namespace content {

class RenderFrameHostImpl;
class RenderFrameProxyHost;
class WebContentsImpl;

// A Portal provides a way to embed a WebContents inside a frame in another
// WebContents. It also provides an API that the owning frame can interact with
// the portal WebContents. The portal can be activated, where the portal
// WebContents replaces the outer WebContents and inherit it as a new Portal.
//
// The Portal is owned by its mojo binding, so it is kept alive as long as the
// other end of the pipe (typically in the renderer) exists.
class CONTENT_EXPORT Portal : public blink::mojom::Portal,
                              public blink::mojom::PortalHost,
                              public FrameTreeNode::Observer,
                              public WebContentsObserver,
                              public WebContentsDelegate {
 public:
  ~Portal() override;

  static bool IsEnabled();

  static Portal* FromToken(const base::UnguessableToken& portal_token);

  // Creates a Portal and binds it to the pipe specified in the |request|. This
  // function creates a strong binding, so the ownership of the Portal is
  // delegated to the binding.
  static Portal* Create(
      RenderFrameHostImpl* owner_render_frame_host,
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client);

  // Creates a portal without binding it to any pipe. Only used in tests.
  static std::unique_ptr<Portal> CreateForTesting(
      RenderFrameHostImpl* owner_render_frame_host);

  static void BindPortalHostReceiver(
      RenderFrameHostImpl* frame,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalHost>
          pending_receiver);

  // Called from a synchronous IPC from the renderer process in order to create
  // the proxy.
  RenderFrameProxyHost* CreateProxyAndAttachPortal();

  // blink::mojom::Portal implementation.
  void Navigate(const GURL& url,
                blink::mojom::ReferrerPtr referrer,
                NavigateCallback callback) override;
  void Activate(blink::TransferableMessage data,
                ActivateCallback callback) override;
  void PostMessageToGuest(
      const blink::TransferableMessage message,
      const base::Optional<url::Origin>& target_origin) override;

  // blink::mojom::PortalHost implementation
  void PostMessageToHost(
      blink::TransferableMessage message,
      const base::Optional<url::Origin>& target_origin) override;

  // FrameTreeNode::Observer overrides.
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

  // WebContentsObserver overrides.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // WebContentsDelegate overrides.
  void LoadingStateChanged(WebContents* source,
                           bool to_different_document) override;
  void PortalWebContentsCreated(WebContents* portal_web_contents) override;

  // Returns the token which uniquely identifies this Portal.
  const base::UnguessableToken& portal_token() const { return portal_token_; }

  // Returns the devtools frame token for the portal's main frame.
  base::UnguessableToken GetDevToolsFrameToken() const;

  // Returns the Portal's WebContents.
  WebContentsImpl* GetPortalContents();

  RenderFrameHostImpl* owner_render_frame_host() {
    return owner_render_frame_host_;
  }

  // Gets/sets the mojo binding. Only used in tests.
  mojo::StrongAssociatedBindingPtr<blink::mojom::Portal>
  GetBindingForTesting() {
    return binding_;
  }
  void SetBindingForTesting(
      mojo::StrongAssociatedBindingPtr<blink::mojom::Portal> binding);
  void SetClientForTesting(
      mojo::AssociatedRemote<blink::mojom::PortalClient> client);

  blink::mojom::PortalClient& client() { return *(client_.get()); }

 private:
  explicit Portal(RenderFrameHostImpl* owner_render_frame_host);

  void SetPortalContents(std::unique_ptr<WebContents> web_contents);

  RenderFrameHostImpl* owner_render_frame_host_;

  // Uniquely identifies the portal, this token is used by the browser process
  // to reference this portal when communicating with the renderer.
  const base::UnguessableToken portal_token_;

  // WeakPtr to StrongBinding.
  mojo::StrongAssociatedBindingPtr<blink::mojom::Portal> binding_;

  // Receives messages from the inner render process.
  mojo::AssociatedReceiver<blink::mojom::PortalHost> portal_host_receiver_;

  // Used to communicate with the HTMLPortalElement in the renderer that
  // hosts this Portal.
  mojo::AssociatedRemote<blink::mojom::PortalClient> client_;

  // When the portal is not attached, the Portal owns its WebContents.
  std::unique_ptr<WebContents> portal_contents_;

  WebContentsImpl* portal_contents_impl_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_H_
