// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace cc {
class Layer;
}

namespace blink {

class AssociatedInterfaceProvider;
class InterfaceRegistry;
class LocalFrame;
class RemoteFrameClient;
struct FrameLoadRequest;

class CORE_EXPORT RemoteFrame final : public Frame,
                                      public mojom::blink::RemoteFrame {
 public:
  // For a description of |inheriting_agent_factory| go see the comment on the
  // Frame constructor.
  RemoteFrame(RemoteFrameClient*,
              Page&,
              FrameOwner*,
              WindowAgentFactory* inheriting_agent_factory,
              InterfaceRegistry*,
              AssociatedInterfaceProvider*);
  ~RemoteFrame() override;

  // Frame overrides:
  void Trace(blink::Visitor*) override;
  void Navigate(const FrameLoadRequest&, WebFrameLoadType) override;
  RemoteSecurityContext* GetSecurityContext() const override;
  bool DetachDocument() override;
  void CheckCompleted() override;
  bool ShouldClose() override;
  void HookBackForwardCacheEviction() override {}
  void RemoveBackForwardCacheEviction() override {}
  void SetIsInert(bool) override;
  void SetInheritedEffectiveTouchAction(TouchAction) override;
  bool BubbleLogicalScrollFromChildFrame(ScrollDirection direction,
                                         ScrollGranularity granularity,
                                         Frame* child) override;
  void DidFocus() override;

  void SetCcLayer(cc::Layer*,
                  bool prevent_contents_opaque_changes,
                  bool is_surface_layer);
  cc::Layer* GetCcLayer() const { return cc_layer_; }
  bool WebLayerHasFixedContentsOpaque() const {
    return prevent_contents_opaque_changes_;
  }

  void AdvanceFocus(WebFocusType, LocalFrame* source);

  void SetView(RemoteFrameView*);
  void CreateView();

  mojom::blink::RemoteFrameHost& GetRemoteFrameHostRemote();

  RemoteFrameView* View() const override;

  RemoteFrameClient* Client() const;

  bool IsIgnoredForHitTest() const;

  void DidChangeVisibleToHitTesting() override;

  void SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      const ParsedFeaturePolicy& parsed_header,
      const FeaturePolicy::FeatureState&);

  // blink::mojom::LocalFrame overrides:
  void WillEnterFullscreen() override;
  void ResetReplicatedContentSecurityPolicy() override;
  void EnforceInsecureNavigationsSet(const WTF::Vector<uint32_t>& set) override;
  void SetReplicatedOrigin(
      const scoped_refptr<const SecurityOrigin>& origin,
      bool is_potentially_trustworthy_unique_origin) override;
  void DispatchLoadEventForFrameOwner() override;

 private:
  // Frame protected overrides:
  void DetachImpl(FrameDetachType) override;

  // Intentionally private to prevent redundant checks when the type is
  // already RemoteFrame.
  bool IsLocalFrame() const override { return false; }
  bool IsRemoteFrame() const override { return true; }

  void DetachChildren();
  void ApplyReplicatedFeaturePolicyHeader();

  static void BindToReceiver(
      blink::RemoteFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver);

  Member<RemoteFrameView> view_;
  Member<RemoteSecurityContext> security_context_;
  cc::Layer* cc_layer_ = nullptr;
  bool prevent_contents_opaque_changes_ = false;
  bool is_surface_layer_ = false;
  ParsedFeaturePolicy feature_policy_header_;

  mojo::AssociatedRemote<mojom::blink::RemoteFrameHost>
      remote_frame_host_remote_;
  mojo::AssociatedReceiver<mojom::blink::RemoteFrame> receiver_{this};
};

inline RemoteFrameView* RemoteFrame::View() const {
  return view_.Get();
}

template <>
struct DowncastTraits<RemoteFrame> {
  static bool AllowFrom(const Frame& frame) { return frame.IsRemoteFrame(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_H_
