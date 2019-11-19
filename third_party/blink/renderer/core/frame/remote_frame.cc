// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "cc/layers/surface_layer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen_options.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

RemoteFrame::RemoteFrame(
    RemoteFrameClient* client,
    Page& page,
    FrameOwner* owner,
    WindowAgentFactory* inheriting_agent_factory,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider)
    : Frame(client,
            page,
            owner,
            MakeGarbageCollected<RemoteWindowProxyManager>(*this),
            inheriting_agent_factory),
      security_context_(MakeGarbageCollected<RemoteSecurityContext>()) {
  dom_window_ = MakeGarbageCollected<RemoteDOMWindow>(*this);

  interface_registry->AddAssociatedInterface(WTF::BindRepeating(
      &RemoteFrame::BindToReceiver, WrapWeakPersistent(this)));

  associated_interface_provider->GetInterface(
      remote_frame_host_remote_.BindNewEndpointAndPassReceiver());

  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
  UpdateVisibleToHitTesting();
  Initialize();
}

RemoteFrame::~RemoteFrame() {
  DCHECK(!view_);
}

void RemoteFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(view_);
  visitor->Trace(security_context_);
  Frame::Trace(visitor);
}

void RemoteFrame::Navigate(const FrameLoadRequest& passed_request,
                           WebFrameLoadType frame_load_type) {
  if (!navigation_rate_limiter().CanProceed())
    return;

  FrameLoadRequest frame_request(passed_request);
  frame_request.SetFrameType(
      IsMainFrame() ? network::mojom::RequestContextFrameType::kTopLevel
                    : network::mojom::RequestContextFrameType::kNested);

  const KURL& url = frame_request.GetResourceRequest().Url();
  if (!frame_request.CanDisplay(url)) {
    if (frame_request.OriginDocument()) {
      frame_request.OriginDocument()->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError,
          "Not allowed to load local resource: " + url.ElidedString()));
    }
    return;
  }

  // The process where this frame actually lives won't have sufficient
  // information to upgrade the url, since it won't have access to the
  // originDocument. Do it now.
  const FetchClientSettingsObject* fetch_client_settings_object = nullptr;
  if (frame_request.OriginDocument()) {
    fetch_client_settings_object = &frame_request.OriginDocument()
                                        ->Fetcher()
                                        ->GetProperties()
                                        .GetFetchClientSettingsObject();
  }
  MixedContentChecker::UpgradeInsecureRequest(
      frame_request.GetResourceRequest(), fetch_client_settings_object,
      frame_request.OriginDocument(), frame_request.GetFrameType());

  bool is_opener_navigation = false;
  bool initiator_frame_has_download_sandbox_flag = false;
  bool initiator_frame_is_ad = false;
  LocalFrame* frame = frame_request.OriginDocument()
                          ? frame_request.OriginDocument()->GetFrame()
                          : nullptr;
  if (frame) {
    is_opener_navigation = frame->Client()->Opener() == this;
    initiator_frame_has_download_sandbox_flag =
        frame->GetSecurityContext() &&
        frame->GetSecurityContext()->IsSandboxed(WebSandboxFlags::kDownloads);
    initiator_frame_is_ad = frame->IsAdSubframe();
    if (passed_request.ClientRedirectReason() !=
        ClientNavigationReason::kNone) {
      probe::FrameRequestedNavigation(frame, this, url,
                                      passed_request.ClientRedirectReason());
    }
  }

  bool current_frame_has_download_sandbox_flag =
      GetSecurityContext() &&
      GetSecurityContext()->IsSandboxed(WebSandboxFlags::kDownloads);
  bool has_download_sandbox_flag = initiator_frame_has_download_sandbox_flag ||
                                   current_frame_has_download_sandbox_flag;

  Client()->Navigate(frame_request.GetResourceRequest(),
                     frame_load_type == WebFrameLoadType::kReplaceCurrentItem,
                     is_opener_navigation, has_download_sandbox_flag,
                     initiator_frame_is_ad, frame_request.GetBlobURLToken());
}

void RemoteFrame::DetachImpl(FrameDetachType type) {
  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  DetachChildren();
  if (!Client())
    return;

  // Clean up the frame's view if needed. A remote frame only has a view if
  // the parent is a local frame.
  if (view_)
    view_->Dispose();
  GetWindowProxyManager()->ClearForClose();
  SetView(nullptr);
  // ... the RemoteDOMWindow will need to be informed of detachment,
  // as otherwise it will keep a strong reference back to this RemoteFrame.
  // That combined with wrappers (owned and kept alive by RemoteFrame) keeping
  // persistent strong references to RemoteDOMWindow will prevent the GCing
  // of all these objects. Break the cycle by notifying of detachment.
  To<RemoteDOMWindow>(dom_window_.Get())->FrameDetached();
  if (cc_layer_)
    SetCcLayer(nullptr, false, false);
  receiver_.reset();
}

bool RemoteFrame::DetachDocument() {
  DetachChildren();
  return !!GetPage();
}

void RemoteFrame::CheckCompleted() {
  // Notify the client so that the corresponding LocalFrame can do the check.
  Client()->CheckCompleted();
}

RemoteSecurityContext* RemoteFrame::GetSecurityContext() const {
  return security_context_.Get();
}

bool RemoteFrame::ShouldClose() {
  // TODO(nasko): Implement running the beforeunload handler in the actual
  // LocalFrame running in a different process and getting back a real result.
  return true;
}

void RemoteFrame::SetIsInert(bool inert) {
  if (inert != is_inert_)
    Client()->SetIsInert(inert);
  is_inert_ = inert;
}

void RemoteFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ != touch_action)
    GetRemoteFrameHostRemote().SetInheritedEffectiveTouchAction(touch_action);
  inherited_effective_touch_action_ = touch_action;
}

bool RemoteFrame::BubbleLogicalScrollFromChildFrame(
    ScrollDirection direction,
    ScrollGranularity granularity,
    Frame* child) {
  DCHECK(child->Client());
  To<LocalFrame>(child)->Client()->BubbleLogicalScrollInParentFrame(
      direction, granularity);
  return false;
}

void RemoteFrame::DidFocus() {
  GetRemoteFrameHostRemote().DidFocusFrame();
}

void RemoteFrame::SetView(RemoteFrameView* view) {
  // Oilpan: as RemoteFrameView performs no finalization actions,
  // no explicit Dispose() of it needed here. (cf. LocalFrameView::Dispose().)
  view_ = view;
}

void RemoteFrame::CreateView() {
  // If the RemoteFrame does not have a LocalFrame parent, there's no need to
  // create a EmbeddedContentView for it.
  if (!DeprecatedLocalOwner())
    return;

  DCHECK(!DeprecatedLocalOwner()->OwnedEmbeddedContentView());

  SetView(MakeGarbageCollected<RemoteFrameView>(this));

  if (OwnerLayoutObject())
    DeprecatedLocalOwner()->SetEmbeddedContentView(view_);
}

mojom::blink::RemoteFrameHost& RemoteFrame::GetRemoteFrameHostRemote() {
  return *remote_frame_host_remote_.get();
}

RemoteFrameClient* RemoteFrame::Client() const {
  return static_cast<RemoteFrameClient*>(Frame::Client());
}

void RemoteFrame::DidChangeVisibleToHitTesting() {
  if (!cc_layer_ || !is_surface_layer_)
    return;

  static_cast<cc::SurfaceLayer*>(cc_layer_)->SetHasPointerEventsNone(
      IsIgnoredForHitTest());
}

void RemoteFrame::SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
    const ParsedFeaturePolicy& parsed_header,
    const FeaturePolicy::FeatureState& opener_feature_state) {
  feature_policy_header_ = parsed_header;
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    DCHECK(opener_feature_state.empty() || IsMainFrame());
    if (OpenerFeatureState().empty()) {
      SetOpenerFeatureState(opener_feature_state);
    }
  }
  ApplyReplicatedFeaturePolicyHeader();
}

void RemoteFrame::WillEnterFullscreen() {
  // This should only ever be called when the FrameOwner is local.
  HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(Owner());

  // Call |requestFullscreen()| on |ownerElement| to make it the pending
  // fullscreen element in anticipation of the coming |didEnterFullscreen()|
  // call.
  //
  // PrefixedForCrossProcessDescendant is necessary because:
  //  - The fullscreen element ready check and other checks should be bypassed.
  //  - |ownerElement| will need :-webkit-full-screen-ancestor style in addition
  //    to :fullscreen.
  //
  // TODO(alexmos): currently, this assumes prefixed requests, but in the
  // future, this should plumb in information about which request type
  // (prefixed or unprefixed) to use for firing fullscreen events.
  Fullscreen::RequestFullscreen(
      *owner_element, FullscreenOptions::Create(),
      Fullscreen::RequestType::kPrefixedForCrossProcessDescendant);
}

void RemoteFrame::ResetReplicatedContentSecurityPolicy() {
  GetSecurityContext()->ResetReplicatedContentSecurityPolicy();
}

void RemoteFrame::EnforceInsecureNavigationsSet(
    const WTF::Vector<uint32_t>& set) {
  GetSecurityContext()->SetInsecureNavigationsSet(set);
}

void RemoteFrame::SetReplicatedOrigin(
    const scoped_refptr<const SecurityOrigin>& origin,
    bool is_potentially_trustworthy_unique_origin) {
  scoped_refptr<SecurityOrigin> security_origin = origin->IsolatedCopy();
  security_origin->SetOpaqueOriginIsPotentiallyTrustworthy(
      is_potentially_trustworthy_unique_origin);
  GetSecurityContext()->SetReplicatedOrigin(security_origin);
  ApplyReplicatedFeaturePolicyHeader();

  // If the origin of a remote frame changed, the accessibility object for the
  // owner element now points to a different child.
  //
  // TODO(dmazzoni, dcheng): there's probably a better way to solve this.
  // Run SitePerProcessAccessibilityBrowserTest.TwoCrossSiteNavigations to
  // ensure an alternate fix works.  http://crbug.com/566222
  FrameOwner* owner = Owner();
  HTMLElement* owner_element = DynamicTo<HTMLFrameOwnerElement>(owner);
  if (owner_element) {
    AXObjectCache* cache = owner_element->GetDocument().ExistingAXObjectCache();
    if (cache)
      cache->ChildrenChanged(owner_element);
  }
}

void RemoteFrame::DispatchLoadEventForFrameOwner() {
  DCHECK(Owner()->IsLocal());
  Owner()->DispatchLoad();
}

bool RemoteFrame::IsIgnoredForHitTest() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  if (!owner || !owner->GetLayoutObject())
    return false;

  return owner->OwnerType() == FrameOwnerElementType::kPortal ||
         !visible_to_hit_testing_;
}

void RemoteFrame::SetCcLayer(cc::Layer* cc_layer,
                             bool prevent_contents_opaque_changes,
                             bool is_surface_layer) {
  DCHECK(Owner());

  if (cc_layer_)
    GraphicsLayer::UnregisterContentsLayer(cc_layer_);
  cc_layer_ = cc_layer;
  prevent_contents_opaque_changes_ = prevent_contents_opaque_changes;
  is_surface_layer_ = is_surface_layer;
  if (cc_layer_) {
    GraphicsLayer::RegisterContentsLayer(cc_layer_);
    if (is_surface_layer) {
      static_cast<cc::SurfaceLayer*>(cc_layer_)->SetHasPointerEventsNone(
          IsIgnoredForHitTest());
    }
  }

  To<HTMLFrameOwnerElement>(Owner())->SetNeedsCompositingUpdate();
}

void RemoteFrame::AdvanceFocus(WebFocusType type, LocalFrame* source) {
  Client()->AdvanceFocus(type, source);
}

void RemoteFrame::DetachChildren() {
  using FrameVector = HeapVector<Member<Frame>>;
  FrameVector children_to_detach;
  children_to_detach.ReserveCapacity(Tree().ChildCount());
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    children_to_detach.push_back(child);
  for (const auto& child : children_to_detach)
    child->Detach(FrameDetachType::kRemove);
}

void RemoteFrame::ApplyReplicatedFeaturePolicyHeader() {
  const FeaturePolicy* parent_feature_policy = nullptr;
  if (Frame* parent_frame = Client()->Parent()) {
    parent_feature_policy =
        parent_frame->GetSecurityContext()->GetFeaturePolicy();
  }
  ParsedFeaturePolicy container_policy;
  if (Owner())
    container_policy = Owner()->GetFramePolicy().container_policy;
  const FeaturePolicy::FeatureState& opener_feature_state =
      OpenerFeatureState();
  GetSecurityContext()->InitializeFeaturePolicy(
      feature_policy_header_, container_policy, parent_feature_policy,
      opener_feature_state.empty() ? nullptr : &opener_feature_state);
}

void RemoteFrame::BindToReceiver(
    blink::RemoteFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver) {
  DCHECK(frame);
  frame->receiver_.Bind(std::move(receiver));
}

}  // namespace blink
