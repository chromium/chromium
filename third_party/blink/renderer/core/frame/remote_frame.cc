// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "cc/layers/surface_layer.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// Maintain a global (statically-allocated) hash map indexed by the the result
// of hashing the |frame_token| passed on creation of a RemoteFrame object.
typedef HeapHashMap<uint64_t, WeakMember<RemoteFrame>> RemoteFramesByTokenMap;
static RemoteFramesByTokenMap& GetRemoteFramesMap() {
  DEFINE_STATIC_LOCAL(Persistent<RemoteFramesByTokenMap>, map,
                      (MakeGarbageCollected<RemoteFramesByTokenMap>()));
  return *map;
}

FloatRect DeNormalizeRect(const gfx::RectF& normalized, const IntRect& base) {
  FloatRect result(normalized);
  result.Scale(base.Width(), base.Height());
  result.MoveBy(FloatPoint(base.Location()));
  return result;
}

}  // namespace

// static
RemoteFrame* RemoteFrame::FromFrameToken(
    const base::UnguessableToken& frame_token) {
  RemoteFramesByTokenMap& remote_frames_map = GetRemoteFramesMap();
  auto it = remote_frames_map.find(base::UnguessableTokenHash()(frame_token));
  return it == remote_frames_map.end() ? nullptr : it->value.Get();
}

// static
RemoteFrame* RemoteFrame::FromFrameToken(const RemoteFrameToken& frame_token) {
  return FromFrameToken(frame_token.value());
}

RemoteFrame::RemoteFrame(
    RemoteFrameClient* client,
    Page& page,
    FrameOwner* owner,
    Frame* parent,
    Frame* previous_sibling,
    FrameInsertType insert_type,
    const base::UnguessableToken& frame_token,
    WindowAgentFactory* inheriting_agent_factory,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider)
    : Frame(client,
            page,
            owner,
            parent,
            previous_sibling,
            insert_type,
            frame_token,
            MakeGarbageCollected<RemoteWindowProxyManager>(*this),
            inheriting_agent_factory),
      interface_registry_(interface_registry
                              ? interface_registry
                              : InterfaceRegistry::GetEmptyInterfaceRegistry()),
      task_runner_(page.GetPageScheduler()
                       ->GetAgentGroupScheduler()
                       .DefaultTaskRunner()) {
  // TODO(crbug.com/1094850): Remove this check once the renderer is correctly
  // handling errors during the creation of HTML portal elements, which would
  // otherwise cause RemoteFrame() being created with empty frame tokens.
  if (!frame_token.is_empty()) {
    auto frame_tracking_result = GetRemoteFramesMap().insert(
        base::UnguessableTokenHash()(frame_token), this);
    CHECK(frame_tracking_result.stored_value) << "Inserting a duplicate item.";
  }

  dom_window_ = MakeGarbageCollected<RemoteDOMWindow>(*this);

  interface_registry->AddAssociatedInterface(WTF::BindRepeating(
      &RemoteFrame::BindToReceiver, WrapWeakPersistent(this)));

  DCHECK(task_runner_);
  associated_interface_provider->GetInterface(
      remote_frame_host_remote_.BindNewEndpointAndPassReceiver(task_runner_));

  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
  UpdateVisibleToHitTesting();
  Initialize();
}

RemoteFrame::~RemoteFrame() {
  DCHECK(!view_);
}

void RemoteFrame::Trace(Visitor* visitor) const {
  visitor->Trace(view_);
  visitor->Trace(security_context_);
  Frame::Trace(visitor);
}

void RemoteFrame::Navigate(FrameLoadRequest& frame_request,
                           WebFrameLoadType frame_load_type) {
  // RemoteFrame::Navigate doesn't support policies like
  // kNavigationPolicyNewForegroundTab - such policies need to be handled via
  // local frames.
  DCHECK_EQ(kNavigationPolicyCurrentTab, frame_request.GetNavigationPolicy());

  if (HTMLFrameOwnerElement* element = DeprecatedLocalOwner())
    element->CancelPendingLazyLoad();

  if (!navigation_rate_limiter().CanProceed())
    return;

  frame_request.SetFrameType(IsMainFrame()
                                 ? mojom::RequestContextFrameType::kTopLevel
                                 : mojom::RequestContextFrameType::kNested);

  const KURL& url = frame_request.GetResourceRequest().Url();
  auto* window = frame_request.GetOriginWindow();
  if (!frame_request.CanDisplay(url)) {
    if (window) {
      window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError,
          "Not allowed to load local resource: " + url.ElidedString()));
    }
    return;
  }

  // The process where this frame actually lives won't have sufficient
  // information to upgrade the url, since it won't have access to the
  // origin context. Do it now.
  const FetchClientSettingsObject* fetch_client_settings_object = nullptr;
  if (window) {
    fetch_client_settings_object =
        &window->Fetcher()->GetProperties().GetFetchClientSettingsObject();
  }
  MixedContentChecker::UpgradeInsecureRequest(
      frame_request.GetResourceRequest(), fetch_client_settings_object, window,
      frame_request.GetFrameType(),
      window ? window->GetFrame()->GetContentSettingsClient() : nullptr);

  // Navigations in portal contexts do not create back/forward entries.
  if (GetPage()->InsidePortal() &&
      frame_load_type == WebFrameLoadType::kStandard) {
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  WebLocalFrame* initiator_frame =
      window ? window->GetFrame()->Client()->GetWebFrame() : nullptr;

  bool is_opener_navigation = false;
  bool initiator_frame_has_download_sandbox_flag = false;
  bool initiator_frame_is_ad = false;

  if (window) {
    is_opener_navigation = window->GetFrame()->Opener() == this;
    initiator_frame_has_download_sandbox_flag =
        window->IsSandboxed(network::mojom::blink::WebSandboxFlags::kDownloads);
    initiator_frame_is_ad = window->GetFrame()->IsAdSubframe();
    if (frame_request.ClientRedirectReason() != ClientNavigationReason::kNone) {
      probe::FrameRequestedNavigation(window->GetFrame(), this, url,
                                      frame_request.ClientRedirectReason(),
                                      kNavigationPolicyCurrentTab);
    }
  }

  Client()->Navigate(frame_request.GetResourceRequest(), initiator_frame,
                     frame_load_type == WebFrameLoadType::kReplaceCurrentItem,
                     is_opener_navigation,
                     initiator_frame_has_download_sandbox_flag,
                     initiator_frame_is_ad, frame_request.GetBlobURLToken(),
                     frame_request.Impression());
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
  main_frame_receiver_.reset();
}

bool RemoteFrame::DetachDocument() {
  DetachChildren();
  return !!GetPage();
}

void RemoteFrame::CheckCompleted() {
  // Notify the client so that the corresponding LocalFrame can do the check.
  GetRemoteFrameHostRemote().CheckCompleted();
}

const RemoteSecurityContext* RemoteFrame::GetSecurityContext() const {
  return &security_context_;
}

bool RemoteFrame::ShouldClose() {
  // TODO(nasko): Implement running the beforeunload handler in the actual
  // LocalFrame running in a different process and getting back a real result.
  return true;
}

void RemoteFrame::SetIsInert(bool inert) {
  if (inert != is_inert_)
    GetRemoteFrameHostRemote().SetIsInert(inert);
  is_inert_ = inert;
}

void RemoteFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ != touch_action)
    GetRemoteFrameHostRemote().SetInheritedEffectiveTouchAction(touch_action);
  inherited_effective_touch_action_ = touch_action;
}

bool RemoteFrame::BubbleLogicalScrollFromChildFrame(
    mojom::blink::ScrollDirection direction,
    ScrollGranularity granularity,
    Frame* child) {
  DCHECK(child->Client());
  To<LocalFrame>(child)
      ->GetLocalFrameHostRemote()
      .BubbleLogicalScrollInParentFrame(direction, granularity);
  return false;
}

void RemoteFrame::RenderFallbackContent() {
  // TODO(ekaramad): If the owner renders its own content, then the current
  // ContentFrame() should detach and free-up the OOPIF process (see
  // https://crbug.com/850223).
  auto* owner = DeprecatedLocalOwner();
  DCHECK(IsA<HTMLObjectElement>(owner));
  owner->RenderFallbackContent(this);
}

void RemoteFrame::AddResourceTimingFromChild(
    mojom::blink::ResourceTimingInfoPtr timing) {
  HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(Owner());
  DCHECK(owner_element);

  // TODO(https://crbug.com/900700): Take a Mojo pending receiver for
  // WorkerTimingContainer for navigation from the calling function.
  DOMWindowPerformance::performance(*owner_element->GetDocument().domWindow())
      ->AddResourceTiming(std::move(timing), owner_element->localName(),
                          /*worker_timing_receiver=*/mojo::NullReceiver(),
                          owner_element->GetDocument().GetExecutionContext());
}

void RemoteFrame::DidStartLoading() {
  SetIsLoading(true);
}

void RemoteFrame::DidStopLoading() {
  SetIsLoading(false);

  // When a subframe finishes loading, the parent should check if *all*
  // subframes have finished loading (which may mean that the parent can declare
  // that the parent itself has finished loading). This remote-subframe-focused
  // code has a local-subframe equivalent in FrameLoader::DidFinishNavigation.
  Frame* parent = Tree().Parent();
  if (parent)
    parent->CheckCompleted();
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

void RemoteFrame::ForwardPostMessage(
    MessageEvent* message_event,
    base::Optional<base::UnguessableToken> cluster_id,
    scoped_refptr<const SecurityOrigin> target_security_origin,
    LocalFrame* source_frame) {
  base::Optional<base::UnguessableToken> source_token;
  if (source_frame)
    source_token = source_frame->GetFrameToken();

  String source_origin = message_event->origin();
  String target_origin = g_empty_string;
  if (target_security_origin)
    target_origin = target_security_origin->ToString();

  GetRemoteFrameHostRemote().RouteMessageEvent(
      source_token, source_origin, target_origin,
      BlinkTransferableMessage::FromMessageEvent(message_event, cluster_id));
}

mojom::blink::RemoteFrameHost& RemoteFrame::GetRemoteFrameHostRemote() {
  return *remote_frame_host_remote_.get();
}

AssociatedInterfaceProvider* RemoteFrame::GetRemoteAssociatedInterfaces() {
  DCHECK(Client());
  return Client()->GetRemoteAssociatedInterfaces();
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
    const FeaturePolicyFeatureState& opener_feature_state) {
  feature_policy_header_ = parsed_header;
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    DCHECK(opener_feature_state.empty() || IsMainFrame());
    if (OpenerFeatureState().empty()) {
      SetOpenerFeatureState(opener_feature_state);
    }
  }
  ApplyReplicatedFeaturePolicyHeader();
}

void RemoteFrame::SetReplicatedSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  security_context_.ResetAndEnforceSandboxFlags(flags);
}

void RemoteFrame::SetInsecureRequestPolicy(
    mojom::blink::InsecureRequestPolicy policy) {
  security_context_.SetInsecureRequestPolicy(policy);
}

void RemoteFrame::SetInsecureNavigationsSet(const WebVector<unsigned>& set) {
  security_context_.SetInsecureNavigationsSet(set);
}

void RemoteFrame::WillEnterFullscreen(
    mojom::blink::FullscreenOptionsPtr request_options) {
  // This should only ever be called when the FrameOwner is local.
  HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(Owner());

  // Call |requestFullscreen()| on |ownerElement| to make it the pending
  // fullscreen element in anticipation of the coming |didEnterFullscreen()|
  // call.
  //
  // ForCrossProcessDescendant is necessary because:
  //  - The fullscreen element ready check and other checks should be bypassed.
  //  - |ownerElement| will need :-webkit-full-screen-ancestor style in addition
  //    to :fullscreen.
  FullscreenRequestType request_type =
      (request_options->is_prefixed ? FullscreenRequestType::kPrefixed
                                    : FullscreenRequestType::kUnprefixed) |
      (request_options->is_xr_overlay ? FullscreenRequestType::kForXrOverlay
                                      : FullscreenRequestType::kNull) |
      FullscreenRequestType::kForCrossProcessDescendant;

  Fullscreen::RequestFullscreen(*owner_element, FullscreenOptions::Create(),
                                request_type);
}

void RemoteFrame::AddReplicatedContentSecurityPolicies(
    WTF::Vector<network::mojom::blink::ContentSecurityPolicyHeaderPtr>
        headers) {
  for (auto& header : headers) {
    GetSecurityContext()->GetContentSecurityPolicy()->AddPolicyFromHeaderValue(
        header->header_value, header->type, header->source);
  }
}

void RemoteFrame::ResetReplicatedContentSecurityPolicy() {
  security_context_.ResetReplicatedContentSecurityPolicy();
}

void RemoteFrame::EnforceInsecureNavigationsSet(
    const WTF::Vector<uint32_t>& set) {
  security_context_.SetInsecureNavigationsSet(set);
}

void RemoteFrame::SetFrameOwnerProperties(
    mojom::blink::FrameOwnerPropertiesPtr properties) {
  Frame::ApplyFrameOwnerProperties(std::move(properties));
}

void RemoteFrame::EnforceInsecureRequestPolicy(
    mojom::blink::InsecureRequestPolicy policy) {
  SetInsecureRequestPolicy(policy);
}

void RemoteFrame::SetReplicatedOrigin(
    const scoped_refptr<const SecurityOrigin>& origin,
    bool is_potentially_trustworthy_unique_origin) {
  scoped_refptr<SecurityOrigin> security_origin = origin->IsolatedCopy();
  security_origin->SetOpaqueOriginIsPotentiallyTrustworthy(
      is_potentially_trustworthy_unique_origin);
  security_context_.SetReplicatedOrigin(security_origin);
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

void RemoteFrame::SetReplicatedAdFrameType(
    mojom::blink::AdFrameType ad_frame_type) {
  if (ad_frame_type_ == mojom::blink::AdFrameType::kNonAd) {
    ad_frame_type_ = ad_frame_type;
  } else {
    DCHECK_EQ(ad_frame_type_, ad_frame_type);
  }
}

void RemoteFrame::SetReplicatedName(const String& name,
                                    const String& unique_name) {
  Tree().SetName(AtomicString(name));
  unique_name_ = unique_name;
}

void RemoteFrame::DispatchLoadEventForFrameOwner() {
  DCHECK(Owner()->IsLocal());
  Owner()->DispatchLoad();
}

void RemoteFrame::Collapse(bool collapsed) {
  FrameOwner* owner = Owner();
  To<HTMLFrameOwnerElement>(owner)->SetCollapsed(collapsed);
}

void RemoteFrame::Focus() {
  FocusImpl();
}

void RemoteFrame::SetHadStickyUserActivationBeforeNavigation(bool value) {
  Frame::SetHadStickyUserActivationBeforeNavigation(value);
}

void RemoteFrame::SetNeedsOcclusionTracking(bool needs_tracking) {
  View()->SetNeedsOcclusionTracking(needs_tracking);
}

void RemoteFrame::BubbleLogicalScroll(mojom::blink::ScrollDirection direction,
                                      ui::ScrollGranularity granularity) {
  Frame* parent_frame = Parent();
  DCHECK(parent_frame);
  DCHECK(parent_frame->IsLocalFrame());

  parent_frame->BubbleLogicalScrollFromChildFrame(direction, granularity, this);
}

void RemoteFrame::UpdateUserActivationState(
    mojom::blink::UserActivationUpdateType update_type,
    mojom::blink::UserActivationNotificationType notification_type) {
  switch (update_type) {
    case mojom::blink::UserActivationUpdateType::kNotifyActivation:
      NotifyUserActivationInFrameTree(notification_type);
      break;
    case mojom::blink::UserActivationUpdateType::kConsumeTransientActivation:
      ConsumeTransientUserActivationInFrameTree();
      break;
    case mojom::blink::UserActivationUpdateType::kClearActivation:
      ClearUserActivationInFrameTree();
      break;
    case mojom::blink::UserActivationUpdateType::
        kNotifyActivationPendingBrowserVerification:
      NOTREACHED() << "Unexpected UserActivationUpdateType from browser";
      break;
  }
}

void RemoteFrame::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  DCHECK(IsA<HTMLFrameOwnerElement>(Owner()));
  Frame::SetEmbeddingToken(embedding_token);
}

void RemoteFrame::SetPageFocus(bool is_focused) {
  WebFrame::FromFrame(this)->View()->SetFocus(is_focused);
}

void RemoteFrame::ScrollRectToVisible(
    const gfx::Rect& rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  Element* owner_element = DeprecatedLocalOwner();
  LayoutObject* owner_object = owner_element->GetLayoutObject();
  if (!owner_object) {
    // The LayoutObject could be nullptr by the time we get here. For instance
    // <iframe>'s style might have been set to 'display: none' right after
    // scrolling starts in the OOPIF's process (see https://crbug.com/777811).
    return;
  }

  // Schedule the scroll.
  PhysicalRect absolute_rect = owner_object->LocalToAncestorRect(
      PhysicalRect(LayoutUnit(rect_to_scroll.x()),
                   LayoutUnit(rect_to_scroll.y()),
                   LayoutUnit(rect_to_scroll.width()),
                   LayoutUnit(rect_to_scroll.height())),
      owner_object->View());

  if (!params->zoom_into_rect ||
      !owner_object->GetDocument().GetFrame()->LocalFrameRoot().IsMainFrame()) {
    owner_object->ScrollRectToVisible(absolute_rect, std::move(params));
    return;
  }

  // ZoomAndScrollToFocusedEditableElementRect will scroll only the layout and
  // visual viewports. Ensure the element is actually visible in the viewport
  // scrolling layer. (i.e. isn't clipped by some other content).
  auto relative_element_bounds = params->relative_element_bounds;
  auto relative_caret_bounds = params->relative_caret_bounds;

  params->stop_at_main_frame_layout_viewport = true;
  absolute_rect =
      owner_object->ScrollRectToVisible(absolute_rect, std::move(params));

  IntRect rect_in_document =
      owner_object->GetDocument()
          .GetFrame()
          ->LocalFrameRoot()
          .View()
          ->RootFrameToDocument(EnclosingIntRect(
              owner_element->GetDocument().View()->ConvertToRootFrame(
                  absolute_rect)));
  IntRect element_bounds_in_document = EnclosingIntRect(
      DeNormalizeRect(relative_element_bounds, rect_in_document));
  IntRect caret_bounds_in_document = EnclosingIntRect(
      DeNormalizeRect(relative_caret_bounds, rect_in_document));

  // This is due to something such as scroll focused editable element into
  // view on Android which also requires an automatic zoom into legible scale.
  // This is handled by main frame's WebView.
  WebViewImpl* web_view =
      static_cast<WebViewImpl*>(WebFrame::FromFrame(this)->View());
  web_view->ZoomAndScrollToFocusedEditableElementRect(
      element_bounds_in_document, caret_bounds_in_document, true);
}

void RemoteFrame::IntrinsicSizingInfoOfChildChanged(
    mojom::blink::IntrinsicSizingInfoPtr info) {
  FrameOwner* owner = Owner();
  // Only communication from HTMLPluginElement-owned subframes is allowed
  // at present. This includes <embed> and <object> tags.
  if (!owner || !owner->IsPlugin())
    return;

  // TODO(https://crbug.com/1044304): Should either remove the native
  // C++ Blink type and use the Mojo type everywhere or typemap the
  // Mojo type to the pre-existing native C++ Blink type.
  IntrinsicSizingInfo sizing_info;
  sizing_info.size = FloatSize(info->size);
  sizing_info.aspect_ratio = FloatSize(info->aspect_ratio);
  sizing_info.has_width = info->has_width;
  sizing_info.has_height = info->has_height;
  View()->SetIntrinsicSizeInfo(sizing_info);

  owner->IntrinsicSizingInfoChanged();
}

// Update the proxy's SecurityContext with new sandbox flags or feature policy
// that were set during navigation. Unlike changes to the FrameOwner, which are
// handled by RemoteFrame::DidUpdateFramePolicy, these changes should be
// considered effective immediately.
//
// These flags / policy are needed on the remote frame's SecurityContext to
// ensure that sandbox flags and feature policy are inherited properly if this
// proxy ever parents a local frame.
void RemoteFrame::DidSetFramePolicyHeaders(
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const WTF::Vector<ParsedFeaturePolicyDeclaration>& parsed_feature_policy) {
  SetReplicatedSandboxFlags(sandbox_flags);
  // Convert from WTF::Vector<ParsedFeaturePolicyDeclaration>
  // to std::vector<ParsedFeaturePolicyDeclaration>, since ParsedFeaturePolicy
  // is an alias for the later.
  //
  // TODO(crbug.com/1047273): Remove this conversion by switching
  // ParsedFeaturePolicy to operate over Vector
  ParsedFeaturePolicy parsed_feature_policy_copy(parsed_feature_policy.size());
  for (size_t i = 0; i < parsed_feature_policy.size(); ++i)
    parsed_feature_policy_copy[i] = parsed_feature_policy[i];
  SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      parsed_feature_policy_copy, FeaturePolicyFeatureState());
}

// Update the proxy's FrameOwner with new sandbox flags and container policy
// that were set by its parent in another process.
//
// Normally, when a frame's sandbox attribute is changed dynamically, the
// frame's FrameOwner is updated with the new sandbox flags right away, while
// the frame's SecurityContext is updated when the frame is navigated and the
// new sandbox flags take effect.
//
// Currently, there is no use case for a proxy's pending FrameOwner sandbox
// flags, so there's no message sent to proxies when the sandbox attribute is
// first updated.  Instead, the active flags are updated when they take effect,
// by OnDidSetActiveSandboxFlags. The proxy's FrameOwner flags are updated here
// with the caveat that the FrameOwner won't learn about updates to its flags
// until they take effect.
void RemoteFrame::DidUpdateFramePolicy(const FramePolicy& frame_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  SECURITY_CHECK(IsA<RemoteFrameOwner>(Owner()));
  To<RemoteFrameOwner>(Owner())->SetFramePolicy(frame_policy);
}

void RemoteFrame::UpdateOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {
  if (auto* web_frame = WebFrame::FromFrame(this)) {
    Frame* opener_frame = nullptr;
    if (opener_frame_token)
      opener_frame = Frame::ResolveFrame(opener_frame_token.value());
    SetOpenerDoNotNotify(opener_frame);
  }
}

IntSize RemoteFrame::GetMainFrameViewportSize() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  DCHECK(owner);
  DCHECK(owner->GetDocument().GetFrame());
  return owner->GetDocument().GetFrame()->GetMainFrameViewportSize();
}

IntPoint RemoteFrame::GetMainFrameScrollOffset() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  DCHECK(owner);
  DCHECK(owner->GetDocument().GetFrame());
  return owner->GetDocument().GetFrame()->GetMainFrameScrollOffset();
}

void RemoteFrame::SetOpener(Frame* opener_frame) {
  if (Opener() == opener_frame)
    return;

  auto* web_frame = WebFrame::FromFrame(this);
  if (web_frame) {
    // A proxy shouldn't normally be disowning its opener.  It is possible to
    // get here when a proxy that is being detached clears its opener, in
    // which case there is no need to notify the browser process.
    if (opener_frame) {
      // Only a LocalFrame (i.e., the caller of window.open) should be able to
      // update another frame's opener.
      DCHECK(opener_frame->IsLocalFrame());
      GetRemoteFrameHostRemote().DidChangeOpener(
          opener_frame ? base::Optional<base::UnguessableToken>(
                             opener_frame->GetFrameToken())
                       : base::nullopt);
    }
  }
  SetOpenerDoNotNotify(opener_frame);
}

void RemoteFrame::UpdateTextAutosizerPageInfo(
    mojom::blink::TextAutosizerPageInfoPtr mojo_remote_page_info) {
  // Only propagate the remote page info if our main frame is remote.
  DCHECK(IsMainFrame());
  Frame* root_frame = GetPage()->MainFrame();
  DCHECK(root_frame->IsRemoteFrame());
  if (*mojo_remote_page_info == GetPage()->TextAutosizerPageInfo())
    return;

  GetPage()->SetTextAutosizerPageInfo(*mojo_remote_page_info);
  TextAutosizer::UpdatePageInfoInAllFrames(root_frame);
}

void RemoteFrame::WasAttachedAsRemoteMainFrame() {
  interface_registry_->AddAssociatedInterface(WTF::BindRepeating(
      &RemoteFrame::BindToMainFrameReceiver, WrapWeakPersistent(this)));
}

viz::FrameSinkId RemoteFrame::GetFrameSinkId() {
  return Client()->GetFrameSinkId();
}

bool RemoteFrame::IsIgnoredForHitTest() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  if (!owner || !owner->GetLayoutObject())
    return false;

  return owner->OwnerType() == mojom::blink::FrameOwnerElementType::kPortal ||
         !visible_to_hit_testing_;
}

void RemoteFrame::SetCcLayer(cc::Layer* cc_layer,
                             bool prevent_contents_opaque_changes,
                             bool is_surface_layer) {
  DCHECK(Owner());

  cc_layer_ = cc_layer;
  prevent_contents_opaque_changes_ = prevent_contents_opaque_changes;
  is_surface_layer_ = is_surface_layer;
  if (cc_layer_) {
    if (is_surface_layer) {
      static_cast<cc::SurfaceLayer*>(cc_layer_)->SetHasPointerEventsNone(
          IsIgnoredForHitTest());
    }
  }
  HTMLFrameOwnerElement* owner = To<HTMLFrameOwnerElement>(Owner());
  owner->SetNeedsCompositingUpdate();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // New layers for remote frames are controlled by Blink's embedder.
    // To ensure the new surface is painted, we need to repaint the frame
    // owner's PaintLayer.
    LayoutBoxModelObject* layout_object = owner->GetLayoutBoxModelObject();
    if (layout_object && layout_object->Layer())
      layout_object->Layer()->SetNeedsRepaint();
  }
}

void RemoteFrame::AdvanceFocus(mojom::blink::FocusType type,
                               LocalFrame* source) {
  GetRemoteFrameHostRemote().AdvanceFocus(type, source->GetFrameToken());
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
  if (Frame* parent_frame = Parent()) {
    parent_feature_policy =
        parent_frame->GetSecurityContext()->GetFeaturePolicy();
  }
  ParsedFeaturePolicy container_policy;
  if (Owner())
    container_policy = Owner()->GetFramePolicy().container_policy;
  const FeaturePolicyFeatureState& opener_feature_state = OpenerFeatureState();
  security_context_.InitializeFeaturePolicy(
      feature_policy_header_, container_policy, parent_feature_policy,
      opener_feature_state.empty() ? nullptr : &opener_feature_state);
}

void RemoteFrame::BindToReceiver(
    RemoteFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver) {
  DCHECK(frame);
  frame->receiver_.Bind(std::move(receiver), frame->task_runner_);
}

void RemoteFrame::BindToMainFrameReceiver(
    RemoteFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteMainFrame> receiver) {
  DCHECK(frame);
  frame->main_frame_receiver_.Bind(std::move(receiver), frame->task_runner_);
}

}  // namespace blink
