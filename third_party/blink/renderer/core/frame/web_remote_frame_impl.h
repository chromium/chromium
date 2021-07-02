// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_REMOTE_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_REMOTE_FRAME_IMPL_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class FrameOwner;
struct FrameVisualProperties;
class RemoteFrame;
class RemoteFrameClientImpl;
enum class WebFrameLoadType;
class WebFrameWidget;
class WebView;
class WindowAgentFactory;

class CORE_EXPORT WebRemoteFrameImpl final
    : public GarbageCollected<WebRemoteFrameImpl>,
      public WebRemoteFrame {
 public:
  static WebRemoteFrameImpl* CreateMainFrame(
      WebView*,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      WebFrame* opener);
  static WebRemoteFrameImpl* CreateForPortalOrFencedFrame(
      mojom::blink::TreeScopeType,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const WebElement& frame_owner);

  WebRemoteFrameImpl(mojom::blink::TreeScopeType,
                     WebRemoteFrameClient*,
                     InterfaceRegistry*,
                     AssociatedInterfaceProvider*,
                     const RemoteFrameToken& frame_token);
  ~WebRemoteFrameImpl() override;

  // WebFrame methods:
  void Close() override;
  WebView* View() const override;

  // WebRemoteFrame methods:
  WebLocalFrame* CreateLocalChild(
      mojom::blink::TreeScopeType,
      const WebString& name,
      const FramePolicy&,
      WebLocalFrameClient*,
      InterfaceRegistry*,
      WebFrame* previous_sibling,
      const WebFrameOwnerProperties&,
      const LocalFrameToken& frame_token,
      WebFrame* opener,
      std::unique_ptr<blink::WebPolicyContainer> policy_container) override;
  WebRemoteFrame* CreateRemoteChild(
      mojom::blink::TreeScopeType,
      const WebString& name,
      const FramePolicy&,
      WebRemoteFrameClient*,
      InterfaceRegistry*,
      AssociatedInterfaceProvider*,
      const RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      WebFrame* opener) override;
  void SetReplicatedOrigin(
      const WebSecurityOrigin&,
      bool is_potentially_trustworthy_opaque_origin) override;
  void SetReplicatedSandboxFlags(
      network::mojom::blink::WebSandboxFlags) override;
  void SetReplicatedName(const WebString& name,
                         const WebString& unique_name) override;
  void SetReplicatedPermissionsPolicyHeader(
      const ParsedPermissionsPolicy& parsed_header) override;
  void SetReplicatedInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy) override;
  void SetReplicatedInsecureNavigationsSet(const WebVector<unsigned>&) override;
  void SetReplicatedIsAdSubframe(bool is_ad_subframe) override;
  void DidStartLoading() override;
  bool IsIgnoredForHitTest() const override;
  void UpdateUserActivationState(
      mojom::blink::UserActivationUpdateType update_type,
      mojom::blink::UserActivationNotificationType notification_type) override;
  void SetHadStickyUserActivationBeforeNavigation(bool value) override;
  v8::Local<v8::Object> GlobalProxy() const override;
  void SynchronizeVisualProperties() override;
  void ResendVisualProperties() override;
  float GetCompositingScaleFactor() override;
  WebString UniqueName() const override;
  const FrameVisualProperties& GetPendingVisualPropertiesForTesting()
      const override;
  bool IsAdSubframe() const override;
  void InitializeCoreFrame(Page&,
                           FrameOwner*,
                           WebFrame* parent,
                           WebFrame* previous_sibling,
                           FrameInsertType,
                           const AtomicString& name,
                           WindowAgentFactory*,
                           const base::UnguessableToken& devtools_frame_token);
  RemoteFrame* GetFrame() const { return frame_.Get(); }

  WebRemoteFrameClient* Client() const { return client_; }

  static WebRemoteFrameImpl* FromFrame(RemoteFrame&);

  void Trace(Visitor*) const;

  gfx::Rect GetCompositingRect();

 private:
  friend class RemoteFrameClientImpl;

  void SetCoreFrame(RemoteFrame*);
  void InitializeFrameVisualProperties(WebFrameWidget* ancestor_widget,
                                       WebView* web_view);

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrameImpl.
  bool IsWebLocalFrame() const override;
  WebLocalFrame* ToWebLocalFrame() override;
  const WebLocalFrame* ToWebLocalFrame() const override;
  bool IsWebRemoteFrame() const override;
  WebRemoteFrame* ToWebRemoteFrame() override;
  const WebRemoteFrame* ToWebRemoteFrame() const override;

  WebRemoteFrameClient* client_;
  // TODO(dcheng): Inline this field directly rather than going through Member.
  Member<RemoteFrameClientImpl> frame_client_;
  Member<RemoteFrame> frame_;

  InterfaceRegistry* const interface_registry_;
  AssociatedInterfaceProvider* const associated_interface_provider_;

  // Oilpan: WebRemoteFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebRemoteFrameImpl> self_keep_alive_;
};

template <>
struct DowncastTraits<WebRemoteFrameImpl> {
  static bool AllowFrom(const WebFrame& frame) {
    return frame.IsWebRemoteFrame();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_REMOTE_FRAME_IMPL_H_
