// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "content/public/browser/frame_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/referrer_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"
#include "ui/base/page_transition_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"
#endif

class GURL;

namespace net {
class IsolationInfo;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content {

class CrossOriginEmbedderPolicyReporter;
class NavigationRequest;
class Navigator;
class RenderFrameHostManager;
class RenderFrameHostImpl;

// An interface for RenderFrameHostImpl to communicate with FrameTreeNode owning
// it (e.g. to initiate or cancel a navigation in the frame).
//
// As main RenderFrameHostImpl can be moved between different FrameTreeNodes
// (i.e.during prerender activations), RenderFrameHostImpl should not reference
// FrameTreeNode directly to prevent accident violation of implicit "associated
// FTN stays the same" assumptions. Instead, a targeted interface is exposed
// instead.
//
// If you need to store information which should persist during prerender
// activations and same-BrowsingContext navigations, consider using
// BrowsingContextState instead.
class RenderFrameHostOwner {
 public:
  RenderFrameHostOwner() = default;
  virtual ~RenderFrameHostOwner() = default;

  // A RenderFrameHost started loading.
  virtual void DidStartLoading(
      LoadingState previous_frame_tree_loading_state) = 0;

  // A RenderFrameHost in this owner stopped loading.
  virtual void DidStopLoading() = 0;

  virtual void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) = 0;

  // Reload the current document in this frame again. Return whether an actual
  // navigation request was created or not.
  virtual bool Reload() = 0;

  virtual Navigator& GetCurrentNavigator() = 0;

  virtual RenderFrameHostManager& GetRenderFrameHostManager() = 0;

  virtual FrameTreeNode* GetOpener() const = 0;

  virtual void SetFocusedFrame(SiteInstanceGroup* source) = 0;

  // Called when the referrer policy changes.
  virtual void DidChangeReferrerPolicy(
      network::mojom::ReferrerPolicy referrer_policy) = 0;

  virtual bool UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) = 0;

  // Called to notify all frames of a page that the history user activation
  // has been consumed, in response to an event in the renderer process.
  virtual void DidConsumeHistoryUserActivation() = 0;

  // Called when document.open occurs, which causes the frame to no longer be in
  // an initial empty document state.
  virtual void DidOpenDocumentInputStream() = 0;

  // Creates a NavigationRequest  for a synchronous navigation that has
  // committed in the renderer process. Those are:
  // - same-document renderer-initiated navigations.
  // - synchronous about:blank navigations.
  virtual std::unique_ptr<NavigationRequest>
  CreateNavigationRequestForSynchronousRendererCommit(
      RenderFrameHostImpl* render_frame_host,
      bool is_same_document,
      const GURL& url,
      const url::Origin& origin,
      const std::optional<GURL>& initiator_base_url,
      const net::IsolationInfo& isolation_info_for_subresources,
      blink::mojom::ReferrerPtr referrer,
      const ui::PageTransition& transition,
      bool should_replace_current_entry,
      const std::string& method,
      bool has_transient_activation,
      bool is_overriding_user_agent,
      const std::vector<GURL>& redirects,
      const GURL& original_url,
      std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
      int http_response_code) = 0;

  // Cancels the navigation owned by the FrameTreeNode.
  // Note: this does not cancel navigations that are owned by the current or
  // speculative RenderFrameHosts.
  virtual void CancelNavigation(NavigationDiscardReason reason) = 0;

  // Reset every non-speculative navigation in this frame, and its descendants.
  // This is called after outermost main frame has been discarded.
  //
  // This takes into account:
  // - Non-pending commit NavigationRequest owned by the FrameTreeNode
  // - Pending commit NavigationRequest owned by the current RenderFrameHost
  virtual void ResetNavigationsForDiscard() = 0;

  // Return the iframe.credentialless attribute value.
  virtual bool Credentialless() const = 0;

#if !BUILDFLAG(IS_ANDROID)
  virtual void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver) = 0;
#endif

  virtual FrameType GetCurrentFrameType() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
