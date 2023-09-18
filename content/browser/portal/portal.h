// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom-forward.h"
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
  explicit Portal(RenderFrameHostImpl* owner_render_frame_host);
  Portal(RenderFrameHostImpl* owner_render_frame_host,
         std::unique_ptr<WebContents> existing_web_contents);
  ~Portal() override;

  static bool IsEnabled();

  static void BindPortalHostReceiver(
      RenderFrameHostImpl* frame,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalHost>
          pending_receiver);

  // Associates this via Mojo with a remote client in the renderer process.
  void Bind(mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
            mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client);

  // Called when it is time for the portal to be deleted, such as when the pipe
  // holding it closes. If this is never called, the owning RenderFrameHostImpl
  // is responsible for deleting this object.
  //
  // This object will be deleted by the time this returns. Any pointers to it
  // are invalid.
  void DestroySelf();

  // Called from a synchronous IPC from the renderer process in order to create
  // the proxy. `remote_frame_interfaces` must not be null.
  RenderFrameProxyHost* CreateProxyAndAttachPortal(
      blink::mojom::RemoteFrameInterfacesFromRendererPtr
          remote_frame_interfaces);

  // Closes the contents associated with this object gracefully, and destroys
  // itself thereafter. This will fire unload and related event handlers.
  // Once closing begins, the Portal interface receiver is closed. The host
  // document can no longer manage the lifetime.
  void Close();

  // blink::mojom::Portal implementation.
  void Navigate(const GURL& url,
                blink::mojom::ReferrerPtr referrer,
                NavigateCallback callback) override;
  void Activate(blink::TransferableMessage data,
                base::TimeTicks activation_time,
                uint64_t trace_id,
                ActivateCallback callback) override;
  void PostMessageToGuest(const blink::TransferableMessage message) override;

  // blink::mojom::PortalHost implementation
  void PostMessageToHost(blink::TransferableMessage message) override;

  // FrameTreeNode::Observer overrides.
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override;

  // WebContentsObserver overrides.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // WebContentsDelegate overrides.
  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) override;
  void PortalWebContentsCreated(WebContents* portal_web_contents) override;
  void CloseContents(WebContents*) override;
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override;
  bool ShouldFocusPageAfterCrash() override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;

  // Returns the token which uniquely identifies this Portal.
  const blink::PortalToken& portal_token() const { return portal_token_; }

  // Returns the devtools frame token for the portal's main frame.
  base::UnguessableToken GetDevToolsFrameToken() const;

  // Returns the Portal's WebContents.
  WebContentsImpl* GetPortalContents() const;
  // Returns the WebContents that hosts this portal.
  WebContentsImpl* GetPortalHostContents() const;

  RenderFrameHostImpl* owner_render_frame_host() {
    return owner_render_frame_host_;
  }

  // Only used in tests.
  blink::mojom::Portal* GetInterceptorForTesting() const {
    return interceptor_.get();
  }

  [[nodiscard]] blink::mojom::Portal* SetInterceptorForTesting(
      std::unique_ptr<blink::mojom::Portal> interceptor) {
    interceptor_ = std::move(interceptor);
    return receiver_.SwapImplForTesting(interceptor_.get());
  }

  blink::mojom::PortalClient& client() { return *(client_.get()); }

  // Returns true if the portal is same-origin with its host.
  bool IsSameOrigin() const;

 private:
  // Manages the relationship between the Portal and its guest WebContents.
  //
  // The WebContents may either be:
  // * owned by this object (via unique_ptr) when it is not attached to the
  //   FrameTreeNode/WebContents tree, e.g. during activation but before
  //   adoption
  // * unowned by this object, in which case it is owned elsewhere, generally
  //   via by WebContentsTreeNode
  //
  // It can transition between these two states. In either state, the Portal
  // must be configured as the portal and delegate of the WebContents.
  //
  // Finally, if the Portal drops its relationship with a WebContents, it must
  // also stop observing the corresponding outer FrameTreeNode.
  class WebContentsHolder {
   public:
    explicit WebContentsHolder(Portal* portal);
    WebContentsHolder(const WebContentsHolder&) = delete;
    ~WebContentsHolder();

    WebContentsHolder& operator=(const WebContentsHolder&) = delete;

    explicit operator bool() const { return contents_; }
    WebContentsImpl& operator*() const { return *contents_; }
    WebContentsImpl* operator->() const { return contents_; }
    WebContentsImpl* get() const { return contents_; }
    bool OwnsContents() const;

    void SetUnowned(WebContentsImpl*);
    void SetOwned(std::unique_ptr<WebContents>);
    void Clear();

    // Maintains a link to the same contents, but yields ownership to the
    // caller.
    std::unique_ptr<WebContents> ReleaseOwnership() {
      DCHECK(OwnsContents());
      if (owned_contents_) {
        owned_contents_->SetOwnerLocationForDebug(absl::nullopt);
      }
      return std::move(owned_contents_);
    }

   private:
    // The outer Portal object.
    raw_ptr<Portal> portal_ = nullptr;

    // Non-null, even when the contents is not owned.
    raw_ptr<WebContentsImpl> contents_ = nullptr;

    // When the portal is not attached, the Portal owns its WebContents.
    // If not null, |owned_contents_| is equal to |contents_|.
    std::unique_ptr<WebContents> owned_contents_;
  };

  std::pair<bool, blink::mojom::PortalActivateResult> CanActivate();
  void ActivateImpl(blink::TransferableMessage data,
                    base::TimeTicks activation_time,
                    uint64_t trace_id,
                    ActivateCallback callback);

  const raw_ptr<RenderFrameHostImpl, DanglingUntriaged>
      owner_render_frame_host_;

  // Uniquely identifies the portal, this token is used by the browser process
  // to reference this portal when communicating with the renderer.
  const blink::PortalToken portal_token_;

  // Receives messages from the outer (host) frame.
  mojo::AssociatedReceiver<blink::mojom::Portal> receiver_{this};

  // Receives messages from the inner render process.
  mojo::AssociatedReceiver<blink::mojom::PortalHost> portal_host_receiver_{
      this};

  // Used to communicate with the HTMLPortalElement in the renderer that
  // hosts this Portal.
  mojo::AssociatedRemote<blink::mojom::PortalClient> client_;

  // When the portal is not attached, the Portal owns its WebContents.
  WebContentsHolder portal_contents_{this};

  // Set when |Close| is called. Destruction will occur shortly thereafter.
  bool is_closing_ = false;

  // Set when portal is activating.
  bool is_activating_ = false;

  // Another implementation of blink::mojom::Portal to bind instead.
  // For use in testing only.
  std::unique_ptr<blink::mojom::Portal> interceptor_;

  base::WeakPtrFactory<Portal> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_H_
