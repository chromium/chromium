// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "base/types/optional_util.h"
#include "cc/layers/surface_layer.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
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
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

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

}  // namespace

// static
RemoteFrame* RemoteFrame::FromFrameToken(const RemoteFrameToken& frame_token) {
  RemoteFramesByTokenMap& remote_frames_map = GetRemoteFramesMap();
  auto it = remote_frames_map.find(RemoteFrameToken::Hasher()(frame_token));
  return it == remote_frames_map.end() ? nullptr : it->value.Get();
}

RemoteFrame::RemoteFrame(
    RemoteFrameClient* client,
    Page& page,
    FrameOwner* owner,
    Frame* parent,
    Frame* previous_sibling,
    FrameInsertType insert_type,
    const RemoteFrameToken& frame_token,
    WindowAgentFactory* inheriting_agent_factory,
    WebFrameWidget* ancestor_widget,
    const base::UnguessableToken& devtools_frame_token,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver)
    : Frame(client,
            page,
            owner,
            parent,
            previous_sibling,
            insert_type,
            frame_token,
            devtools_frame_token,
            MakeGarbageCollected<RemoteWindowProxyManager>(
                page.GetAgentGroupScheduler().Isolate(),
                *this),
            inheriting_agent_factory),
      // TODO(samans): Investigate if it is safe to delay creation of this
      // object until a FrameSinkId is provided.
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()),
      ancestor_widget_(ancestor_widget),
      task_runner_(page.GetPageScheduler()
                       ->GetAgentGroupScheduler()
                       .DefaultTaskRunner()) {
  auto frame_tracking_result = GetRemoteFramesMap().insert(
      RemoteFrameToken::Hasher()(frame_token), this);
  CHECK(frame_tracking_result.stored_value) << "Inserting a duplicate item.";

  dom_window_ = MakeGarbageCollected<RemoteDOMWindow>(*this);

  DCHECK(task_runner_);
  remote_frame_host_remote_.Bind(std::move(remote_frame_host), task_runner_);
  receiver_.Bind(std::move(receiver), task_runner_);

  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
  UpdateVisibleToHitTesting();
  Initialize();
  if (ancestor_widget)
    compositing_helper_ = std::make_unique<ChildFrameCompositingHelper>(this);
}

RemoteFrame::~RemoteFrame() {
  DCHECK(!view_);
}

void RemoteFrame::DetachAndDispose() {
  DCHECK(!IsMainFrame());
  Detach(FrameDetachType::kRemove);
}

void RemoteFrame::Trace(Visitor* visitor) const {
  visitor->Trace(view_);
  visitor->Trace(security_context_);
  visitor->Trace(remote_frame_host_remote_);
  visitor->Trace(receiver_);
  visitor->Trace(main_frame_receiver_);
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

  // The only navigation paths which do not have an origin window are drag and
  // drop navigations, but they never navigate remote frames.
  DCHECK(window);

  // Note that even if |window| is not null, it could have just been detached
  // (so window->GetFrame() is null). This can happen for a form submission, if
  // the frame containing the form has been deleted in between.

  if (!frame_request.CanDisplay(url)) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        "Not allowed to load local resource: " + url.ElidedString()));
    return;
  }

  // The process where this frame actually lives won't have sufficient
  // information to upgrade the url, since it won't have access to the
  // origin context. Do it now.
  const FetchClientSettingsObject* fetch_client_settings_object =
      &window->Fetcher()->GetProperties().GetFetchClientSettingsObject();
  MixedContentChecker::UpgradeInsecureRequest(
      frame_request.GetResourceRequest(), fetch_client_settings_object, window,
      frame_request.GetFrameType(),
      window->GetFrame() ? window->GetFrame()->GetContentSettingsClient()
                         : nullptr);

  if (NavigationShouldReplaceCurrentHistoryEntry(frame_load_type))
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

  bool is_opener_navigation = false;
  bool initiator_frame_has_download_sandbox_flag = false;
  bool initiator_frame_is_ad = false;
  bool is_ad_script_in_stack = false;

  std::optional<LocalFrameToken> initiator_frame_token =
      base::OptionalFromPtr(frame_request.GetInitiatorFrameToken());
  mojo::PendingRemote<mojom::blink::NavigationStateKeepAliveHandle>
      initiator_navigation_state_keep_alive_handle =
          frame_request.TakeInitiatorNavigationStateKeepAliveHandle();

  // |initiator_frame_token| and |initiator_navigation_state_keep_alive_handle|
  // should either be both specified or both null.
  DCHECK(!initiator_frame_token ==
         !initiator_navigation_state_keep_alive_handle);

  initiator_frame_has_download_sandbox_flag =
      window->IsSandboxed(network::mojom::blink::WebSandboxFlags::kDownloads);
  if (window->GetFrame()) {
    is_opener_navigation = window->GetFrame()->Opener() == this;
    initiator_frame_is_ad = window->GetFrame()->IsAdFrame();
    is_ad_script_in_stack = window->GetFrame()->IsAdScriptInStack();

    probe::FrameRequestedNavigation(window->GetFrame(), this, url,
                                    frame_request.GetClientNavigationReason(),
                                    kNavigationPolicyCurrentTab);

    if (!initiator_frame_token) {
      initiator_frame_token = window->GetFrame()->GetLocalFrameToken();
      initiator_navigation_state_keep_alive_handle =
          window->GetFrame()->IssueKeepAliveHandle();
    }
  }

  // TODO(https://crbug.com/1173409 and https://crbug.com/1059959): Check that
  // we always have valid |initiator_frame_token| and
  // |initiator_navigation_state_keep_alive_handle|.
  ResourceRequest& request = frame_request.GetResourceRequest();
  DCHECK(request.RequestorOrigin().get());

  auto params = mojom::blink::OpenURLParams::New();
  params->url = url;
  params->initiator_origin = request.RequestorOrigin();
  if ((url.IsAboutBlankURL() || url.IsAboutSrcdocURL()) &&
      !frame_request.GetRequestorBaseURL().IsEmpty()) {
    params->initiator_base_url = frame_request.GetRequestorBaseURL();
  }
  params->post_body =
      blink::GetRequestBodyForWebURLRequest(WrappedResourceRequest(request));
  DCHECK_EQ(!!params->post_body, request.HttpMethod().Utf8() == "POST");
  params->extra_headers =
      blink::GetWebURLRequestHeadersAsString(WrappedResourceRequest(request));
  params->referrer = mojom::blink::Referrer::New(
      KURL(NullURL(), request.ReferrerString()), request.GetReferrerPolicy());
  params->is_form_submission = !!frame_request.Form();
  params->disposition = ui::mojom::blink::WindowOpenDisposition::CURRENT_TAB;
  params->should_replace_current_entry =
      frame_load_type == WebFrameLoadType::kReplaceCurrentItem;
  params->user_gesture = request.HasUserGesture();
  params->triggering_event_info = mojom::blink::TriggeringEventInfo::kUnknown;
  params->blob_url_token = frame_request.GetBlobURLToken();
  params->href_translate =
      String(frame_request.HrefTranslate().Latin1().c_str());
  params->initiator_navigation_state_keep_alive_handle =
      std::move(initiator_navigation_state_keep_alive_handle);
  params->initiator_frame_token =
      base::OptionalFromPtr(base::OptionalToPtr(initiator_frame_token));
  params->source_location = network::mojom::blink::SourceLocation::New();

  std::unique_ptr<SourceLocation> source_location =
      frame_request.TakeSourceLocation();
  if (!source_location->IsUnknown()) {
    params->source_location->url =
        source_location->Url() ? source_location->Url() : "";
    params->source_location->line = source_location->LineNumber();
    params->source_location->column = source_location->ColumnNumber();
  }
  params->storage_access_api_status = window->GetStorageAccessApiStatus();

  params->impression = frame_request.Impression();

  // Note: For the AdFrame/Sandbox download policy here it only covers the case
  // where the navigation initiator frame is ad. The download_policy may be
  // further augmented in RenderFrameProxyHost::OnOpenURL if the navigating
  // frame is ad or sandboxed.
  params->download_policy.ApplyDownloadFramePolicy(
      is_opener_navigation, request.HasUserGesture(),
      request.RequestorOrigin()->CanAccess(
          GetSecurityContext()->GetSecurityOrigin()),
      initiator_frame_has_download_sandbox_flag,
      initiator_frame_is_ad);

  params->initiator_activation_and_ad_status =
      GetNavigationInitiatorActivationAndAdStatus(request.HasUserGesture(),
                                                  initiator_frame_is_ad,
                                                  is_ad_script_in_stack);

  params->is_container_initiated = frame_request.IsContainerInitiated();
  params->has_rel_opener = frame_request.GetWindowFeatures().explicit_opener;

  GetRemoteFrameHostRemote().OpenURL(std::move(params));
}

bool RemoteFrame::NavigationShouldReplaceCurrentHistoryEntry(
    WebFrameLoadType frame_load_type) const {
  // Fenced Frame contexts do not create back/forward entries.
  // TODO(https:/crbug.com/1197384, https://crbug.com/1190644): We may want to
  // support a prerender in RemoteFrame.
  return IsInFencedFrameTree();
}

bool RemoteFrame::DetachImpl(FrameDetachType type) {
  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;

  if (!DetachChildren())
    return false;

  // Clean up the frame's view if needed. A remote frame only has a view if
  // the parent is a local frame.
  if (view_)
    view_->Dispose();
  SetView(nullptr);
  // ... the RemoteDOMWindow will need to be informed of detachment,
  // as otherwise it will keep a strong reference back to this RemoteFrame.
  // That combined with wrappers (owned and kept alive by RemoteFrame) keeping
  // persistent strong references to RemoteDOMWindow will prevent the GCing
  // of all these objects. Break the cycle by notifying of detachment.
  To<RemoteDOMWindow>(dom_window_.Get())->FrameDetached();
  if (cc_layer_)
    SetCcLayer(nullptr, false);
  receiver_.reset();
  main_frame_receiver_.reset();

  return true;
}

const scoped_refptr<cc::Layer>& RemoteFrame::GetCcLayer() {
  return cc_layer_;
}

void RemoteFrame::SetCcLayer(scoped_refptr<cc::Layer> layer,
                             bool is_surface_layer) {
  // |ancestor_widget_| can be null if this is a proxy for a remote
  // main frame, or a subframe of that proxy. However, we should not be setting
  // a layer on such a proxy (the layer is used for embedding a child proxy).
  DCHECK(ancestor_widget_);
  DCHECK(Owner());

  cc_layer_ = std::move(layer);
  is_surface_layer_ = is_surface_layer;
  if (cc_layer_ && is_surface_layer_) {
    static_cast<cc::SurfaceLayer&>(*cc_layer_)
        .SetHasPointerEventsNone(IsIgnoredForHitTest());
  }

  HTMLFrameOwnerElement* owner = To<HTMLFrameOwnerElement>(Owner());
  owner->SetNeedsCompositingUpdate();

  // Schedule an animation so that a new frame is produced with the updated
  // layer, otherwise this local root's visible content may not be up to date.
  owner->GetDocument().GetFrame()->View()->ScheduleAnimation();
}

SkBitmap* RemoteFrame::GetSadPageBitmap() {
  return Platform::Current()->GetSadPageBitmap();
}

bool RemoteFrame::DetachDocument() {
  return DetachChildren();
}

void RemoteFrame::CheckCompleted() {
  // Notify the client so that the corresponding LocalFrame can do the check.
  GetRemoteFrameHostRemote().CheckCompleted();
}

const RemoteSecurityContext* RemoteFrame::GetSecurityContext() const {
  return &security_context_;
}

bool RemoteFrame::ShouldClose() {
  // TODO(crbug.com/1407078): Implement running the beforeunload handler in the
  // actual LocalFrame running in a different process and getting back a real
  // result.
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

void RemoteFrame::RenderFallbackContent() {
  Frame::RenderFallbackContent();
}

void RemoteFrame::AddResourceTimingFromChild(
    mojom::blink::ResourceTimingInfoPtr timing) {
  HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(Owner());
  DCHECK(owner_element);
  owner_element->AddResourceTiming(std::move(timing));
}

void RemoteFrame::DidStartLoading() {
  // If this proxy was created for a frame that hasn't yet finished loading,
  // let the renderer know so it can also mark the proxy as loading. See
  // https://crbug.com/916137.
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
    BlinkTransferableMessage transferable_message,
    LocalFrame* source_frame,
    scoped_refptr<const SecurityOrigin> source_security_origin,
    scoped_refptr<const SecurityOrigin> target_security_origin) {
  std::optional<blink::LocalFrameToken> source_token;
  if (source_frame)
    source_token = source_frame->GetLocalFrameToken();

  String target_origin = target_security_origin
                             ? target_security_origin->ToString()
                             : g_empty_string;

  GetRemoteFrameHostRemote().RouteMessageEvent(
      source_token, source_security_origin, target_origin,
      std::move(transferable_message));
}

bool RemoteFrame::IsRemoteFrameHostRemoteBound() {
  return remote_frame_host_remote_.is_bound();
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

  static_cast<cc::SurfaceLayer&>(*cc_layer_)
      .SetHasPointerEventsNone(IsIgnoredForHitTest());
}

void RemoteFrame::SetReplicatedPermissionsPolicyHeader(
    const ParsedPermissionsPolicy& parsed_header) {
  permissions_policy_header_ = parsed_header;
  ApplyReplicatedPermissionsPolicyHeader();
}

void RemoteFrame::SetReplicatedSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  security_context_.ResetAndEnforceSandboxFlags(flags);
}

void RemoteFrame::SetInsecureRequestPolicy(
    mojom::blink::InsecureRequestPolicy policy) {
  security_context_.SetInsecureRequestPolicy(policy);
}

void RemoteFrame::FrameRectsChanged(const gfx::Size& local_frame_size,
                                    const gfx::Rect& rect_in_local_root) {
  pending_visual_properties_.rect_in_local_root = rect_in_local_root;
  pending_visual_properties_.local_frame_size = local_frame_size;
  SynchronizeVisualProperties();
}

void RemoteFrame::InitializeFrameVisualProperties(
    const FrameVisualProperties& properties) {
  pending_visual_properties_ = properties;
  SynchronizeVisualProperties();
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
      (request_options->prefers_status_bar
           ? FullscreenRequestType::kForXrArWithCamera
           : FullscreenRequestType::kNull) |
      FullscreenRequestType::kForCrossProcessDescendant;

  Fullscreen::RequestFullscreen(*owner_element, FullscreenOptions::Create(),
                                request_type);
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
  ApplyReplicatedPermissionsPolicyHeader();

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

bool RemoteFrame::IsAdFrame() const {
  return is_ad_frame_;
}

void RemoteFrame::SetReplicatedIsAdFrame(bool is_ad_frame) {
  is_ad_frame_ = is_ad_frame;
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
  LocalFrame* parent_frame = nullptr;
  if (auto* parent = DynamicTo<LocalFrame>(Parent())) {
    parent_frame = parent;
  } else {
    // This message can be received by an embedded frame tree's placeholder
    // RemoteFrame in which case Parent() is not connected to the outer frame
    // tree.
    auto* owner_element = DynamicTo<HTMLFrameOwnerElement>(Owner());
    DCHECK(owner_element);
    parent_frame = owner_element->GetDocument().GetFrame();
  }

  DCHECK(parent_frame);
  parent_frame->BubbleLogicalScrollFromChildFrame(direction, granularity, this);
}

void RemoteFrame::UpdateUserActivationState(
    mojom::blink::UserActivationUpdateType update_type,
    mojom::blink::UserActivationNotificationType notification_type) {
  switch (update_type) {
    case mojom::blink::UserActivationUpdateType::kNotifyActivation:
      NotifyUserActivationInFrameTree(notification_type);
      break;
    case mojom::blink::UserActivationUpdateType::kNotifyActivationStickyOnly:
      NotifyUserActivationInFrameTreeStickyOnly();
      break;
    case mojom::blink::UserActivationUpdateType::kConsumeTransientActivation:
      ConsumeTransientUserActivationInFrameTree();
      break;
    case mojom::blink::UserActivationUpdateType::kClearActivation:
      ClearUserActivationInFrameTree();
      break;
    case mojom::blink::UserActivationUpdateType::
        kNotifyActivationPendingBrowserVerification:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected UserActivationUpdateType from browser";
      break;
  }
}

void RemoteFrame::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  DCHECK(IsA<HTMLFrameOwnerElement>(Owner()));
  Frame::SetEmbeddingToken(embedding_token);
}

void RemoteFrame::SetPageFocus(bool is_focused) {
  WebViewImpl* web_view =
      To<WebViewImpl>(WebFrame::FromCoreFrame(this)->View());
  if (is_focused) {
    web_view->SetIsActive(true);
  }
  web_view->SetPageFocus(is_focused);
}

void RemoteFrame::ScrollRectToVisible(
    const gfx::RectF& rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  Element* owner_element = DeprecatedLocalOwner();
  LayoutObject* owner_object = owner_element->GetLayoutObject();
  if (!owner_object) {
    // The LayoutObject could be nullptr by the time we get here. For instance
    // <iframe>'s style might have been set to 'display: none' right after
    // scrolling starts in the OOPIF's process (see https://crbug.com/777811).
    return;
  }

  scroll_into_view_util::ConvertParamsToParentFrame(
      params, rect_to_scroll, *owner_object, *owner_object->View());

  PhysicalRect absolute_rect = owner_object->LocalToAncestorRect(
      PhysicalRect::EnclosingRect(rect_to_scroll), owner_object->View());

  scroll_into_view_util::ScrollRectToVisible(*owner_object, absolute_rect,
                                             std::move(params),
                                             /*from_remote_frame=*/true);
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
  sizing_info.size = info->size;
  sizing_info.aspect_ratio = info->aspect_ratio;
  sizing_info.has_width = info->has_width;
  sizing_info.has_height = info->has_height;
  View()->SetIntrinsicSizeInfo(sizing_info);

  owner->IntrinsicSizingInfoChanged();
}

// Update the proxy's SecurityContext with new sandbox flags or permissions
// policy that were set during navigation. Unlike changes to the FrameOwner,
// which are handled by RemoteFrame::DidUpdateFramePolicy, these changes should
// be considered effective immediately.
//
// These flags / policy are needed on the remote frame's SecurityContext to
// ensure that sandbox flags and permissions policy are inherited properly if
// this proxy ever parents a local frame.
void RemoteFrame::DidSetFramePolicyHeaders(
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const WTF::Vector<ParsedPermissionsPolicyDeclaration>&
        parsed_permissions_policy) {
  SetReplicatedSandboxFlags(sandbox_flags);
  // Convert from WTF::Vector<ParsedPermissionsPolicyDeclaration>
  // to std::vector<ParsedPermissionsPolicyDeclaration>, since
  // ParsedPermissionsPolicy is an alias for the later.
  //
  // TODO(crbug.com/1047273): Remove this conversion by switching
  // ParsedPermissionsPolicy to operate over Vector
  ParsedPermissionsPolicy parsed_permissions_policy_copy(
      parsed_permissions_policy.size());
  for (wtf_size_t i = 0; i < parsed_permissions_policy.size(); ++i)
    parsed_permissions_policy_copy[i] = parsed_permissions_policy[i];
  SetReplicatedPermissionsPolicyHeader(parsed_permissions_policy_copy);
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
    const std::optional<blink::FrameToken>& opener_frame_token) {
  Frame* opener_frame = nullptr;
  if (opener_frame_token)
    opener_frame = Frame::ResolveFrame(opener_frame_token.value());
  SetOpenerDoNotNotify(opener_frame);
}

gfx::Size RemoteFrame::GetOutermostMainFrameSize() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  DCHECK(owner);
  DCHECK(owner->GetDocument().GetFrame());
  return owner->GetDocument().GetFrame()->GetOutermostMainFrameSize();
}

gfx::Point RemoteFrame::GetOutermostMainFrameScrollPosition() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  DCHECK(owner);
  DCHECK(owner->GetDocument().GetFrame());
  return owner->GetDocument().GetFrame()->GetOutermostMainFrameScrollPosition();
}

void RemoteFrame::SetOpener(Frame* opener_frame) {
  if (Opener() == opener_frame)
    return;

  // A proxy shouldn't normally be disowning its opener.  It is possible to
  // get here when a proxy that is being detached clears its opener, in
  // which case there is no need to notify the browser process.
  if (opener_frame) {
    // Only a LocalFrame (i.e., the caller of window.open) should be able to
    // update another frame's opener.
    DCHECK(opener_frame->IsLocalFrame());
    GetRemoteFrameHostRemote().DidChangeOpener(
        opener_frame
            ? std::optional<blink::LocalFrameToken>(
                  opener_frame->GetFrameToken().GetAs<LocalFrameToken>())
            : std::nullopt);
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

void RemoteFrame::WasAttachedAsRemoteMainFrame(
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteMainFrame> main_frame) {
  main_frame_receiver_.Bind(std::move(main_frame), task_runner_);
}

const viz::LocalSurfaceId& RemoteFrame::GetLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

void RemoteFrame::SetCcLayerForTesting(scoped_refptr<cc::Layer> layer,
                                       bool is_surface_layer) {
  SetCcLayer(layer, is_surface_layer);
}

viz::FrameSinkId RemoteFrame::GetFrameSinkId() {
  return frame_sink_id_;
}

void RemoteFrame::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id,
                                 bool allow_paint_holding) {
  remote_process_gone_ = false;

  // The same ParentLocalSurfaceIdAllocator cannot provide LocalSurfaceIds for
  // two different frame sinks, so recreate it here.
  if (frame_sink_id_ != frame_sink_id) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  frame_sink_id_ = frame_sink_id;

  // Resend the FrameRects and allocate a new viz::LocalSurfaceId when the view
  // changes.
  ResendVisualPropertiesInternal(
      allow_paint_holding
          ? ChildFrameCompositingHelper::AllowPaintHolding::kYes
          : ChildFrameCompositingHelper::AllowPaintHolding::kNo);
}

void RemoteFrame::ChildProcessGone() {
  remote_process_gone_ = true;
  compositing_helper_->ChildFrameGone(
      ancestor_widget_->GetOriginalScreenInfo().device_scale_factor);
}

bool RemoteFrame::IsIgnoredForHitTest() const {
  HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
  if (!owner || !owner->GetLayoutObject())
    return false;

  return !visible_to_hit_testing_;
}

void RemoteFrame::AdvanceFocus(mojom::blink::FocusType type,
                               LocalFrame* source) {
  GetRemoteFrameHostRemote().AdvanceFocus(type, source->GetLocalFrameToken());
}

bool RemoteFrame::DetachChildren() {
  using FrameVector = HeapVector<Member<Frame>>;
  FrameVector children_to_detach;
  children_to_detach.reserve(Tree().ChildCount());
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    children_to_detach.push_back(child);
  for (const auto& child : children_to_detach)
    child->Detach(FrameDetachType::kRemove);

  return !!Client();
}

void RemoteFrame::ApplyReplicatedPermissionsPolicyHeader() {
  const PermissionsPolicy* parent_permissions_policy = nullptr;
  if (Frame* parent_frame = Parent()) {
    parent_permissions_policy =
        parent_frame->GetSecurityContext()->GetPermissionsPolicy();
  }
  ParsedPermissionsPolicy container_policy;
  if (Owner())
    container_policy = Owner()->GetFramePolicy().container_policy;
  security_context_.InitializePermissionsPolicy(
      permissions_policy_header_, container_policy, parent_permissions_policy);
}

bool RemoteFrame::SynchronizeVisualProperties(
    bool propagate,
    ChildFrameCompositingHelper::AllowPaintHolding allow_paint_holding) {
  if (!GetFrameSinkId().is_valid() || remote_process_gone_)
    return false;

  auto capture_sequence_number_changed =
      (sent_visual_properties_ &&
       sent_visual_properties_->capture_sequence_number !=
           pending_visual_properties_.capture_sequence_number)
          ? ChildFrameCompositingHelper::CaptureSequenceNumberChanged::kYes
          : ChildFrameCompositingHelper::CaptureSequenceNumberChanged::kNo;

  if (view_) {
    pending_visual_properties_.compositor_viewport =
        view_->GetCompositingRect();
    pending_visual_properties_.compositing_scale_factor =
        view_->GetCompositingScaleFactor();
  }

  bool synchronized_props_changed =
      !sent_visual_properties_ ||
      sent_visual_properties_->auto_resize_enabled !=
          pending_visual_properties_.auto_resize_enabled ||
      sent_visual_properties_->min_size_for_auto_resize !=
          pending_visual_properties_.min_size_for_auto_resize ||
      sent_visual_properties_->max_size_for_auto_resize !=
          pending_visual_properties_.max_size_for_auto_resize ||
      sent_visual_properties_->local_frame_size !=
          pending_visual_properties_.local_frame_size ||
      sent_visual_properties_->rect_in_local_root.size() !=
          pending_visual_properties_.rect_in_local_root.size() ||
      sent_visual_properties_->screen_infos !=
          pending_visual_properties_.screen_infos ||
      sent_visual_properties_->zoom_level !=
          pending_visual_properties_.zoom_level ||
      sent_visual_properties_->css_zoom_factor !=
          pending_visual_properties_.css_zoom_factor ||
      sent_visual_properties_->page_scale_factor !=
          pending_visual_properties_.page_scale_factor ||
      sent_visual_properties_->compositing_scale_factor !=
          pending_visual_properties_.compositing_scale_factor ||
      sent_visual_properties_->cursor_accessibility_scale_factor !=
          pending_visual_properties_.cursor_accessibility_scale_factor ||
      sent_visual_properties_->is_pinch_gesture_active !=
          pending_visual_properties_.is_pinch_gesture_active ||
      sent_visual_properties_->visible_viewport_size !=
          pending_visual_properties_.visible_viewport_size ||
      sent_visual_properties_->compositor_viewport !=
          pending_visual_properties_.compositor_viewport ||
      sent_visual_properties_->root_widget_viewport_segments !=
          pending_visual_properties_.root_widget_viewport_segments ||
      sent_visual_properties_->capture_sequence_number !=
          pending_visual_properties_.capture_sequence_number;

  if (synchronized_props_changed)
    parent_local_surface_id_allocator_->GenerateId();
  pending_visual_properties_.local_surface_id = GetLocalSurfaceId();

  viz::SurfaceId surface_id(frame_sink_id_,
                            pending_visual_properties_.local_surface_id);
  DCHECK(ancestor_widget_);
  DCHECK(surface_id.is_valid());
  DCHECK(!remote_process_gone_);

  compositing_helper_->SetSurfaceId(surface_id, capture_sequence_number_changed,
                                    allow_paint_holding);

  bool rect_changed = !sent_visual_properties_ ||
                      sent_visual_properties_->rect_in_local_root !=
                          pending_visual_properties_.rect_in_local_root;
  bool visual_properties_changed = synchronized_props_changed || rect_changed;

  if (visual_properties_changed && propagate) {
    GetRemoteFrameHostRemote().SynchronizeVisualProperties(
        pending_visual_properties_);
    RecordSentVisualProperties();
  }

  return visual_properties_changed;
}

void RemoteFrame::RecordSentVisualProperties() {
  sent_visual_properties_ = pending_visual_properties_;
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "RenderFrameProxy::SynchronizeVisualProperties Send Message",
      TRACE_ID_GLOBAL(
          pending_visual_properties_.local_surface_id.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "message",
      "FrameHostMsg_SynchronizeVisualProperties", "local_surface_id",
      pending_visual_properties_.local_surface_id.ToString());
}

void RemoteFrame::ResendVisualProperties() {
  ResendVisualPropertiesInternal(
      ChildFrameCompositingHelper::AllowPaintHolding::kNo);
}

void RemoteFrame::ResendVisualPropertiesInternal(
    ChildFrameCompositingHelper::AllowPaintHolding allow_paint_holding) {
  sent_visual_properties_ = std::nullopt;
  SynchronizeVisualProperties(/*propagate=*/true, allow_paint_holding);
}

void RemoteFrame::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (!parent_local_surface_id_allocator_->UpdateFromChild(
          metadata.local_surface_id.value_or(viz::LocalSurfaceId()))) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void RemoteFrame::SetViewportIntersection(
    const mojom::blink::ViewportIntersectionState& intersection_state) {
  std::optional<FrameVisualProperties> visual_properties;
  if (SynchronizeVisualProperties(/*propagate=*/false)) {
    visual_properties.emplace(pending_visual_properties_);
    RecordSentVisualProperties();
  }
  GetRemoteFrameHostRemote().UpdateViewportIntersection(
      intersection_state.Clone(), visual_properties);
}

void RemoteFrame::UpdateCompositedLayerBounds() {
  if (cc_layer_)
    cc_layer_->SetBounds(pending_visual_properties_.local_frame_size);
}

void RemoteFrame::DidChangeScreenInfos(
    const display::ScreenInfos& screen_infos) {
  pending_visual_properties_.screen_infos = screen_infos;
  SynchronizeVisualProperties();
}

void RemoteFrame::ZoomFactorChanged(double zoom_factor) {
  // zoom_factor includes device scale factor, browser zoom, and css zoom.
  WebViewImpl* view = GetPage()->GetChromeClient().GetWebView();
  double device_scale_factor = view->ZoomFactorForViewportLayout();
  if (Owner() && Owner()->IsLocal()) {
    DCHECK(ancestor_widget_);
    double zoom_level = ancestor_widget_->GetZoomLevel();
    pending_visual_properties_.zoom_level = zoom_level;
    double browser_zoom_factor = view->ZoomLevelToZoomFactor(zoom_level);
    pending_visual_properties_.css_zoom_factor =
        zoom_factor / (device_scale_factor * browser_zoom_factor);
  } else {
    pending_visual_properties_.zoom_level =
        ZoomFactorToZoomLevel(zoom_factor / device_scale_factor);
    pending_visual_properties_.css_zoom_factor = 1.0;
  }
  SynchronizeVisualProperties();
}

void RemoteFrame::DidChangeRootViewportSegments(
    const std::vector<gfx::Rect>& root_widget_viewport_segments) {
  pending_visual_properties_.root_widget_viewport_segments =
      std::move(root_widget_viewport_segments);
  SynchronizeVisualProperties();
}

void RemoteFrame::PageScaleFactorChanged(float page_scale_factor,
                                         bool is_pinch_gesture_active) {
  pending_visual_properties_.page_scale_factor = page_scale_factor;
  pending_visual_properties_.is_pinch_gesture_active = is_pinch_gesture_active;
  SynchronizeVisualProperties();
}

void RemoteFrame::DidChangeVisibleViewportSize(
    const gfx::Size& visible_viewport_size) {
  pending_visual_properties_.visible_viewport_size = visible_viewport_size;
  SynchronizeVisualProperties();
}

void RemoteFrame::UpdateCaptureSequenceNumber(
    uint32_t capture_sequence_number) {
  pending_visual_properties_.capture_sequence_number = capture_sequence_number;
  SynchronizeVisualProperties();
}

void RemoteFrame::CursorAccessibilityScaleFactorChanged(float scale_factor) {
  pending_visual_properties_.cursor_accessibility_scale_factor = scale_factor;
  SynchronizeVisualProperties();
}

void RemoteFrame::EnableAutoResize(const gfx::Size& min_size,
                                   const gfx::Size& max_size) {
  pending_visual_properties_.auto_resize_enabled = true;
  pending_visual_properties_.min_size_for_auto_resize = min_size;
  pending_visual_properties_.max_size_for_auto_resize = max_size;
  SynchronizeVisualProperties();
}

void RemoteFrame::DisableAutoResize() {
  pending_visual_properties_.auto_resize_enabled = false;
  SynchronizeVisualProperties();
}

void RemoteFrame::CreateRemoteChild(
    const RemoteFrameToken& token,
    const std::optional<FrameToken>& opener_frame_token,
    mojom::blink::TreeScopeType tree_scope_type,
    mojom::blink::FrameReplicationStatePtr replication_state,
    mojom::blink::FrameOwnerPropertiesPtr owner_properties,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    mojom::blink::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces) {
  Client()->CreateRemoteChild(
      token, opener_frame_token, tree_scope_type, std::move(replication_state),
      std::move(owner_properties), is_loading, devtools_frame_token,
      std::move(remote_frame_interfaces));
}

void RemoteFrame::CreateRemoteChildren(
    Vector<mojom::blink::CreateRemoteChildParamsPtr> params) {
  Client()->CreateRemoteChildren(params);
}

void RemoteFrame::ForwardFencedFrameEventToEmbedder(
    const WTF::String& event_type) {
  // This will also CHECK if the conversion to HTMLFrameOwnerElement fails.
  CHECK(To<HTMLFrameOwnerElement>(Owner())->IsHTMLFencedFrameElement());
  static_cast<HTMLFencedFrameElement*>(Owner())->DispatchFencedEvent(
      event_type);
}

}  // namespace blink
