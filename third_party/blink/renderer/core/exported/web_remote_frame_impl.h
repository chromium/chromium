// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_REMOTE_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_REMOTE_FRAME_IMPL_H_

#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace cc {
class Layer;
}

namespace blink {

class FrameOwner;
class RemoteFrame;
class RemoteFrameClientImpl;
enum class WebFrameLoadType;
class WebView;
struct WebRect;
struct WebScrollIntoViewParams;

class CORE_EXPORT WebRemoteFrameImpl final
    : public GarbageCollectedFinalized<WebRemoteFrameImpl>,
      public WebRemoteFrame {
 public:
  static WebRemoteFrameImpl* Create(WebTreeScopeType, WebRemoteFrameClient*);
  static WebRemoteFrameImpl* CreateMainFrame(WebView*,
                                             WebRemoteFrameClient*,
                                             WebFrame* opener = nullptr);

  ~WebRemoteFrameImpl() override;

  // WebFrame methods:
  void Close() override;
  WebRect VisibleContentRect() const override;
  WebView* View() const override;
  void StopLoading() override;

  // WebRemoteFrame methods:
  WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                  const WebString& name,
                                  WebSandboxFlags,
                                  WebLocalFrameClient*,
                                  blink::InterfaceRegistry*,
                                  WebFrame* previous_sibling,
                                  const ParsedFeaturePolicy&,
                                  const WebFrameOwnerProperties&,
                                  FrameOwnerElementType,
                                  WebFrame* opener) override;
  WebRemoteFrame* CreateRemoteChild(WebTreeScopeType,
                                    const WebString& name,
                                    WebSandboxFlags,
                                    const ParsedFeaturePolicy&,
                                    FrameOwnerElementType,
                                    WebRemoteFrameClient*,
                                    WebFrame* opener) override;
  void SetCcLayer(cc::Layer*,
                  bool prevent_contents_opaque_changes,
                  bool is_surface_layer) override;
  void SetReplicatedOrigin(
      const WebSecurityOrigin&,
      bool is_potentially_trustworthy_opaque_origin) override;
  void SetReplicatedSandboxFlags(WebSandboxFlags) override;
  void SetReplicatedName(const WebString&) override;
  void SetReplicatedFeaturePolicyHeader(
      const ParsedFeaturePolicy& parsed_header) override;
  void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      WebContentSecurityPolicyType,
      WebContentSecurityPolicySource) override;
  void ResetReplicatedContentSecurityPolicy() override;
  void SetReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) override;
  void SetReplicatedInsecureNavigationsSet(
      const std::vector<unsigned>&) override;
  void ForwardResourceTimingToParent(const WebResourceTimingInfo&) override;
  void DispatchLoadEventForFrameOwner() override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  bool IsIgnoredForHitTest() const override;
  void WillEnterFullscreen() override;
  void UpdateUserActivationState(UserActivationUpdateType) override;
  void ScrollRectToVisible(const WebRect&,
                           const WebScrollIntoViewParams&) override;
  void BubbleLogicalScroll(WebScrollDirection direction,
                           WebScrollGranularity granularity) override;
  void IntrinsicSizingInfoChanged(const WebIntrinsicSizingInfo&) override;
  void SetHasReceivedUserGestureBeforeNavigation(bool value) override;
  v8::Local<v8::Object> GlobalProxy() const override;
  WebRect GetCompositingRect() override;
  void RenderFallbackContent() const override;

  void InitializeCoreFrame(Page&, FrameOwner*, const AtomicString& name);
  RemoteFrame* GetFrame() const { return frame_.Get(); }

  WebRemoteFrameClient* Client() const { return client_; }

  static WebRemoteFrameImpl* FromFrame(RemoteFrame&);

  void Trace(blink::Visitor*);

 private:
  friend class RemoteFrameClientImpl;

  WebRemoteFrameImpl(WebTreeScopeType, WebRemoteFrameClient*);

  void SetCoreFrame(RemoteFrame*);
  void ApplyReplicatedFeaturePolicyHeader();

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrameImpl.
  bool IsWebLocalFrame() const override;
  WebLocalFrame* ToWebLocalFrame() override;
  bool IsWebRemoteFrame() const override;
  WebRemoteFrame* ToWebRemoteFrame() override;

  WebRemoteFrameClient* client_;
  // TODO(dcheng): Inline this field directly rather than going through Member.
  Member<RemoteFrameClientImpl> frame_client_;
  Member<RemoteFrame> frame_;

  ParsedFeaturePolicy feature_policy_header_;

  // Oilpan: WebRemoteFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebRemoteFrameImpl> self_keep_alive_;
};

DEFINE_TYPE_CASTS(WebRemoteFrameImpl,
                  WebFrame,
                  frame,
                  frame->IsWebRemoteFrame(),
                  frame.IsWebRemoteFrame());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_REMOTE_FRAME_IMPL_H_
