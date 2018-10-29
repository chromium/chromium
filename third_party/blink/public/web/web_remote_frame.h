// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/web/web_frame.h"
#include "v8/include/v8.h"

namespace cc {
class Layer;
}

namespace blink {

enum class WebTreeScopeType;
class InterfaceRegistry;
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
                                             WebRemoteFrameClient*);

  BLINK_EXPORT static WebRemoteFrame*
  CreateMainFrame(WebView*, WebRemoteFrameClient*, WebFrame* opener = nullptr);

  // Specialized factory methods to allow the embedder to replicate the frame
  // tree between processes.
  // TODO(dcheng): The embedder currently does not replicate local frames in
  // insertion order, so the local child version takes |previous_sibling| to
  // ensure that it is inserted into the correct location in the list of
  // children. If |previous_sibling| is null, the child is inserted at the
  // beginning.
  virtual WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                          const WebString& name,
                                          WebSandboxFlags,
                                          WebLocalFrameClient*,
                                          blink::InterfaceRegistry*,
                                          WebFrame* previous_sibling,
                                          const ParsedFeaturePolicy&,
                                          const WebFrameOwnerProperties&,
                                          FrameOwnerElementType,
                                          WebFrame* opener) = 0;

  virtual WebRemoteFrame* CreateRemoteChild(WebTreeScopeType,
                                            const WebString& name,
                                            WebSandboxFlags,
                                            const ParsedFeaturePolicy&,
                                            FrameOwnerElementType,
                                            WebRemoteFrameClient*,
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

  virtual void SetReplicatedFeaturePolicyHeader(
      const ParsedFeaturePolicy& parsed_header) = 0;

  // Adds |header| to the set of replicated CSP headers.
  virtual void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      WebContentSecurityPolicyType,
      WebContentSecurityPolicySource) = 0;

  // Resets replicated CSP headers to an empty set.
  virtual void ResetReplicatedContentSecurityPolicy() = 0;

  // Set frame enforcement of insecure request policy replicated from another
  // process.
  virtual void SetReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) = 0;
  virtual void SetReplicatedInsecureNavigationsSet(
      const std::vector<unsigned>&) = 0;

  // Reports resource timing info for a navigation in this frame.
  virtual void ForwardResourceTimingToParent(const WebResourceTimingInfo&) = 0;

  virtual void DispatchLoadEventForFrameOwner() = 0;

  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Returns true if this frame should be ignored during hittesting.
  virtual bool IsIgnoredForHitTest() const = 0;

  // This is called in OOPIF scenarios when an element contained in this
  // frame is about to enter fullscreen.  This frame's owner
  // corresponds to the HTMLFrameOwnerElement to be fullscreened. Calling
  // this prepares FullscreenController to enter fullscreen for that frame
  // owner.
  virtual void WillEnterFullscreen() = 0;

  // Update the user activation state in appropriate part of this frame's
  // "local" frame tree (ancestors-only vs all-nodes).
  virtual void UpdateUserActivationState(UserActivationUpdateType) = 0;

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
  virtual void BubbleLogicalScroll(WebScrollDirection direction,
                                   WebScrollGranularity granularity) = 0;

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
