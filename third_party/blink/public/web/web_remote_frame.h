// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_

#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/events/types/scroll_types.h"
#include "v8/include/v8.h"

namespace cc {
class Layer;
}

namespace blink {

enum class WebTreeScopeType;
class AssociatedInterfaceProvider;
class InterfaceRegistry;
class WebElement;
class WebLocalFrameClient;
class WebRemoteFrameClient;
class WebString;
class WebView;
struct WebIntrinsicSizingInfo;
struct WebRect;
struct WebResourceTimingInfo;
struct WebScrollIntoViewParams;

class WebRemoteFrame : public WebFrame {
 public:
  // Factory methods for creating a WebRemoteFrame. The WebRemoteFrameClient
  // argument must be non-null for all creation methods.
  BLINK_EXPORT static WebRemoteFrame* Create(WebTreeScopeType,
                                             WebRemoteFrameClient*,
                                             InterfaceRegistry*,
                                             AssociatedInterfaceProvider*);

  BLINK_EXPORT static WebRemoteFrame* CreateMainFrame(
      WebView*,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      WebFrame* opener);

  // Also performs core initialization to associate the created remote frame
  // with the provided <portal> element.
  BLINK_EXPORT static WebRemoteFrame* CreateForPortal(
      WebTreeScopeType,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const WebElement& portal_element);

  // Specialized factory methods to allow the embedder to replicate the frame
  // tree between processes.
  // TODO(dcheng): The embedder currently does not replicate local frames in
  // insertion order, so the local child version takes |previous_sibling| to
  // ensure that it is inserted into the correct location in the list of
  // children. If |previous_sibling| is null, the child is inserted at the
  // beginning.
  virtual WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                          const WebString& name,
                                          const FramePolicy&,
                                          WebLocalFrameClient*,
                                          blink::InterfaceRegistry*,
                                          WebFrame* previous_sibling,
                                          const WebFrameOwnerProperties&,
                                          FrameOwnerElementType,
                                          WebFrame* opener) = 0;

  virtual WebRemoteFrame* CreateRemoteChild(WebTreeScopeType,
                                            const WebString& name,
                                            const FramePolicy&,
                                            FrameOwnerElementType,
                                            WebRemoteFrameClient*,
                                            blink::InterfaceRegistry*,
                                            AssociatedInterfaceProvider*,
                                            WebFrame* opener) = 0;

  // Layer for the in-process compositor.
  virtual void SetCcLayer(cc::Layer*,
                          bool prevent_contents_opaque_changes,
                          bool is_surface_layer) = 0;

  // Set security origin replicated from another process.
  virtual void SetReplicatedOrigin(
      const WebSecurityOrigin&,
      bool is_potentially_trustworthy_opaque_origin) = 0;

  // Set sandbox flags replicated from another process.
  virtual void SetReplicatedSandboxFlags(WebSandboxFlags) = 0;

  // Set frame |name| replicated from another process.
  virtual void SetReplicatedName(const WebString&) = 0;

  // Sets the FeaturePolicy header and the FeatureState (from opener) for the
  // main frame. Once a non-empty |opener_feature_state| is set, it can no
  // longer be modified (due to the fact that the original opener which passed
  // down the FeatureState cannot be modified either).
  virtual void SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      const ParsedFeaturePolicy& parsed_header,
      const FeaturePolicy::FeatureState& opener_feature_state) = 0;

  // Adds |header| to the set of replicated CSP headers.
  virtual void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      network::mojom::ContentSecurityPolicyType,
      WebContentSecurityPolicySource) = 0;

  // Resets replicated CSP headers to an empty set.
  virtual void ResetReplicatedContentSecurityPolicy() = 0;

  // Set frame enforcement of insecure request policy replicated from another
  // process.
  virtual void SetReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) = 0;
  virtual void SetReplicatedInsecureNavigationsSet(
      const WebVector<unsigned>&) = 0;

  // Reports resource timing info for a navigation in this frame.
  virtual void ForwardResourceTimingToParent(const WebResourceTimingInfo&) = 0;

  virtual void SetNeedsOcclusionTracking(bool) = 0;

  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Returns true if this frame should be ignored during hittesting.
  virtual bool IsIgnoredForHitTest() const = 0;

  // Update the user activation state in appropriate part of this frame's
  // "local" frame tree (ancestors-only vs all-nodes).
  virtual void UpdateUserActivationState(UserActivationUpdateType) = 0;

  // Transfers user activation state from |source_frame| to this frame, which
  // must be in the same frame tree as |source_frame|.
  virtual void TransferUserActivationFrom(
      blink::WebRemoteFrame* source_frame) = 0;

  virtual void SetHasReceivedUserGestureBeforeNavigation(bool value) = 0;

  // Scrolls the given rectangle into view. This kicks off the recursive scroll
  // into visible starting from the frame's owner element. The coordinates of
  // the rect are absolute (transforms removed) with respect to the frame in
  // OOPIF process. The parameters are sent by the OOPIF local root and can be
  // used to properly chain the recursive scrolling between the two processes.
  virtual void ScrollRectToVisible(const WebRect&,
                                   const WebScrollIntoViewParams&) = 0;

  // Continues to bubble logical scroll that reached the local root in the child
  // frame's process. Scroll bubbling continues from the frame owner element.
  virtual void BubbleLogicalScroll(
      WebScrollDirection direction,
      ui::input_types::ScrollGranularity granularity) = 0;

  virtual void IntrinsicSizingInfoChanged(const WebIntrinsicSizingInfo&) = 0;

  virtual WebRect GetCompositingRect() = 0;

  // When a cross-process navigation or loading fails, the browser notifies the
  // parent process to render its own fallback content if any. This only occurs
  // if the owner element is capable of rendering its own fallback (e.g.,
  // <object>).
  virtual void RenderFallbackContent() const = 0;

 protected:
  explicit WebRemoteFrame(WebTreeScopeType scope) : WebFrame(scope) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrame.
  bool IsWebLocalFrame() const override = 0;
  WebLocalFrame* ToWebLocalFrame() override = 0;
  bool IsWebRemoteFrame() const override = 0;
  WebRemoteFrame* ToWebRemoteFrame() override = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_
