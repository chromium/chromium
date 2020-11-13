// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_

#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-shared.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-shared.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/events/types/scroll_types.h"
#include "v8/include/v8.h"

namespace cc {
class Layer;
}

namespace blink {

namespace mojom {
enum class TreeScopeType;
}
class AssociatedInterfaceProvider;
class InterfaceRegistry;
class WebElement;
class WebLocalFrameClient;
class WebRemoteFrameClient;
class WebString;
class WebView;
struct FramePolicy;
struct WebFrameOwnerProperties;
struct WebRect;

class WebRemoteFrame : public WebFrame {
 public:
  // Factory methods for creating a WebRemoteFrame. The WebRemoteFrameClient
  // argument must be non-null for all creation methods.
  BLINK_EXPORT static WebRemoteFrame* Create(
      mojom::TreeScopeType,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const base::UnguessableToken& frame_token);

  BLINK_EXPORT static WebRemoteFrame* CreateMainFrame(
      WebView*,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const base::UnguessableToken& frame_token,
      WebFrame* opener);

  // Also performs core initialization to associate the created remote frame
  // with the provided <portal> element.
  BLINK_EXPORT static WebRemoteFrame* CreateForPortal(
      mojom::TreeScopeType,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const base::UnguessableToken& frame_token,
      const WebElement& portal_element);

  // Specialized factory methods to allow the embedder to replicate the frame
  // tree between processes.
  // TODO(dcheng): The embedder currently does not replicate local frames in
  // insertion order, so the local child version takes |previous_sibling| to
  // ensure that it is inserted into the correct location in the list of
  // children. If |previous_sibling| is null, the child is inserted at the
  // beginning.
  virtual WebLocalFrame* CreateLocalChild(
      mojom::TreeScopeType,
      const WebString& name,
      const FramePolicy&,
      WebLocalFrameClient*,
      blink::InterfaceRegistry*,
      WebFrame* previous_sibling,
      const WebFrameOwnerProperties&,
      mojom::FrameOwnerElementType,
      const base::UnguessableToken& frame_token,
      WebFrame* opener,
      std::unique_ptr<blink::WebPolicyContainer> policy_container) = 0;

  virtual WebRemoteFrame* CreateRemoteChild(
      mojom::TreeScopeType,
      const WebString& name,
      const FramePolicy&,
      mojom::FrameOwnerElementType,
      WebRemoteFrameClient*,
      blink::InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const base::UnguessableToken& frame_token,
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
  virtual void SetReplicatedSandboxFlags(network::mojom::WebSandboxFlags) = 0;

  // Set frame |name| and |unique_name| replicated from another process.
  virtual void SetReplicatedName(const WebString& name,
                                 const WebString& unique_name) = 0;

  // Sets the FeaturePolicy header and the FeatureState (from opener) for the
  // main frame. Once a non-empty |opener_feature_state| is set, it can no
  // longer be modified (due to the fact that the original opener which passed
  // down the FeatureState cannot be modified either).
  virtual void SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      const ParsedFeaturePolicy& parsed_header,
      const FeaturePolicyFeatureState& opener_feature_state) = 0;

  // Adds |header| to the set of replicated CSP headers.
  virtual void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      network::mojom::ContentSecurityPolicyType,
      network::mojom::ContentSecurityPolicySource) = 0;

  // Resets replicated CSP headers to an empty set.
  virtual void ResetReplicatedContentSecurityPolicy() = 0;

  // Set frame enforcement of insecure request policy replicated from another
  // process.
  virtual void SetReplicatedInsecureRequestPolicy(
      mojom::InsecureRequestPolicy) = 0;
  virtual void SetReplicatedInsecureNavigationsSet(
      const WebVector<unsigned>&) = 0;

  virtual void SetReplicatedAdFrameType(
      blink::mojom::AdFrameType ad_frame_type) = 0;

  virtual void DidStartLoading() = 0;

  // Returns true if this frame should be ignored during hittesting.
  virtual bool IsIgnoredForHitTest() const = 0;

  // Update the user activation state in appropriate part of this frame's
  // "local" frame tree (ancestors-only vs all-nodes).
  //
  // The |notification_type| parameter is used for histograms, only for the case
  // |update_state == kNotifyActivation|.
  virtual void UpdateUserActivationState(
      mojom::UserActivationUpdateType update_type,
      mojom::UserActivationNotificationType notification_type) = 0;

  virtual void SetHadStickyUserActivationBeforeNavigation(bool value) = 0;

  virtual WebRect GetCompositingRect() = 0;

  // Unique name is an opaque identifier for maintaining association with
  // session restore state for this frame.
  virtual WebString UniqueName() const = 0;

  RemoteFrameToken GetRemoteFrameToken() const {
    return RemoteFrameToken(GetFrameToken());
  }

 protected:
  explicit WebRemoteFrame(mojom::TreeScopeType scope,
                          const base::UnguessableToken& frame_token)
      : WebFrame(scope, frame_token) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrame.
  bool IsWebLocalFrame() const override = 0;
  WebLocalFrame* ToWebLocalFrame() override = 0;
  bool IsWebRemoteFrame() const override = 0;
  WebRemoteFrame* ToWebRemoteFrame() override = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_H_
