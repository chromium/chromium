// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client_impl.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen_options.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
FloatRect DeNormalizeRect(const WebFloatRect& normalized, const IntRect& base) {
  FloatRect result = normalized;
  result.Scale(base.Width(), base.Height());
  result.MoveBy(FloatPoint(base.Location()));
  return result;
}
}  // namespace

WebRemoteFrame* WebRemoteFrame::Create(WebTreeScopeType scope,
                                       WebRemoteFrameClient* client) {
  return WebRemoteFrameImpl::Create(scope, client);
}

WebRemoteFrame* WebRemoteFrame::CreateMainFrame(WebView* web_view,
                                                WebRemoteFrameClient* client,
                                                WebFrame* opener) {
  return WebRemoteFrameImpl::CreateMainFrame(web_view, client, opener);
}

WebRemoteFrameImpl* WebRemoteFrameImpl::Create(WebTreeScopeType scope,
                                               WebRemoteFrameClient* client) {
  WebRemoteFrameImpl* frame = new WebRemoteFrameImpl(scope, client);
  return frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::CreateMainFrame(
    WebView* web_view,
    WebRemoteFrameClient* client,
    WebFrame* opener) {
  WebRemoteFrameImpl* frame =
      new WebRemoteFrameImpl(WebTreeScopeType::kDocument, client);
  frame->SetOpener(opener);
  Page& page = *static_cast<WebViewImpl*>(web_view)->GetPage();
  // It would be nice to DCHECK that the main frame is not set yet here.
  // Unfortunately, there is an edge case with a pending RenderFrameHost that
  // violates this: the embedder may create a pending RenderFrameHost for
  // navigating to a new page in a popup. If the navigation ends up redirecting
  // to a site that requires a process swap, it doesn't go through the standard
  // swapping path and instead directly overwrites the main frame.
  // TODO(dcheng): Remove the need for this and strongly enforce this condition
  // with a DCHECK.
  frame->InitializeCoreFrame(page, nullptr, g_null_atom);
  return frame;
}

WebRemoteFrameImpl::~WebRemoteFrameImpl() = default;

void WebRemoteFrameImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_client_);
  visitor->Trace(frame_);
  WebFrame::TraceFrames(visitor, this);
}

bool WebRemoteFrameImpl::IsWebLocalFrame() const {
  return false;
}

WebLocalFrame* WebRemoteFrameImpl::ToWebLocalFrame() {
  NOTREACHED();
  return nullptr;
}

bool WebRemoteFrameImpl::IsWebRemoteFrame() const {
  return true;
}

WebRemoteFrame* WebRemoteFrameImpl::ToWebRemoteFrame() {
  return this;
}

void WebRemoteFrameImpl::Close() {
  WebRemoteFrame::Close();

  self_keep_alive_.Clear();
}

WebRect WebRemoteFrameImpl::VisibleContentRect() const {
  NOTREACHED();
  return WebRect();
}

WebView* WebRemoteFrameImpl::View() const {
  if (!GetFrame()) {
    return nullptr;
  }
  DCHECK(GetFrame()->GetPage());
  return GetFrame()->GetPage()->GetChromeClient().GetWebView();
}

void WebRemoteFrameImpl::StopLoading() {
  // TODO(dcheng,japhet): Calling this method should stop loads
  // in all subframes, both remote and local.
}

WebLocalFrame* WebRemoteFrameImpl::CreateLocalChild(
    WebTreeScopeType scope,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    WebFrame* previous_sibling,
    const ParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties,
    FrameOwnerElementType frame_owner_element_type,
    WebFrame* opener) {
  WebLocalFrameImpl* child =
      WebLocalFrameImpl::Create(scope, client, interface_registry, opener);
  InsertAfter(child, previous_sibling);
  RemoteFrameOwner* owner = RemoteFrameOwner::Create(
      static_cast<SandboxFlags>(sandbox_flags), container_policy,
      frame_owner_properties, frame_owner_element_type);
  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, name);
  DCHECK(child->GetFrame());
  return child;
}

void WebRemoteFrameImpl::InitializeCoreFrame(Page& page,
                                             FrameOwner* owner,
                                             const AtomicString& name) {
  SetCoreFrame(RemoteFrame::Create(frame_client_.Get(), page, owner));
  GetFrame()->CreateView();
  frame_->Tree().SetName(name);
}

WebRemoteFrame* WebRemoteFrameImpl::CreateRemoteChild(
    WebTreeScopeType scope,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    const ParsedFeaturePolicy& container_policy,
    FrameOwnerElementType frame_owner_element_type,
    WebRemoteFrameClient* client,
    WebFrame* opener) {
  WebRemoteFrameImpl* child = WebRemoteFrameImpl::Create(scope, client);
  child->SetOpener(opener);
  AppendChild(child);
  RemoteFrameOwner* owner = RemoteFrameOwner::Create(
      static_cast<SandboxFlags>(sandbox_flags), container_policy,
      WebFrameOwnerProperties(), frame_owner_element_type);
  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, name);
  return child;
}

void WebRemoteFrameImpl::SetCcLayer(cc::Layer* layer,
                                    bool prevent_contents_opaque_changes,
                                    bool is_surface_layer) {
  GetFrame()->SetCcLayer(layer, prevent_contents_opaque_changes,
                         is_surface_layer);
}

void WebRemoteFrameImpl::SetCoreFrame(RemoteFrame* frame) {
  frame_ = frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::FromFrame(RemoteFrame& frame) {
  if (!frame.Client())
    return nullptr;
  RemoteFrameClientImpl* client =
      static_cast<RemoteFrameClientImpl*>(frame.Client());
  return client->GetWebFrame();
}

void WebRemoteFrameImpl::SetReplicatedOrigin(
    const WebSecurityOrigin& origin,
    bool is_potentially_trustworthy_opaque_origin) {
  DCHECK(GetFrame());
  scoped_refptr<SecurityOrigin> security_origin = origin.Get()->IsolatedCopy();
  security_origin->SetOpaqueOriginIsPotentiallyTrustworthy(
      is_potentially_trustworthy_opaque_origin);
  GetFrame()->GetSecurityContext()->SetReplicatedOrigin(security_origin);
  ApplyReplicatedFeaturePolicyHeader();

  // If the origin of a remote frame changed, the accessibility object for the
  // owner element now points to a different child.
  //
  // TODO(dmazzoni, dcheng): there's probably a better way to solve this.
  // Run SitePerProcessAccessibilityBrowserTest.TwoCrossSiteNavigations to
  // ensure an alternate fix works.  http://crbug.com/566222
  FrameOwner* owner = GetFrame()->Owner();
  if (owner && owner->IsLocal()) {
    HTMLElement* owner_element = ToHTMLFrameOwnerElement(owner);
    AXObjectCache* cache = owner_element->GetDocument().ExistingAXObjectCache();
    if (cache)
      cache->ChildrenChanged(owner_element);
  }
}

void WebRemoteFrameImpl::SetReplicatedSandboxFlags(WebSandboxFlags flags) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->ResetSandboxFlags();
  GetFrame()->GetSecurityContext()->EnforceSandboxFlags(
      static_cast<SandboxFlags>(flags));
}

void WebRemoteFrameImpl::SetReplicatedName(const WebString& name) {
  DCHECK(GetFrame());
  GetFrame()->Tree().SetName(name);
}

void WebRemoteFrameImpl::SetReplicatedFeaturePolicyHeader(
    const ParsedFeaturePolicy& parsed_header) {
  feature_policy_header_ = parsed_header;
  ApplyReplicatedFeaturePolicyHeader();
}

void WebRemoteFrameImpl::ApplyReplicatedFeaturePolicyHeader() {
  FeaturePolicy* parent_feature_policy = nullptr;
  if (Parent()) {
    Frame* parent_frame = GetFrame()->Client()->Parent();
    parent_feature_policy =
        parent_frame->GetSecurityContext()->GetFeaturePolicy();
  }
  ParsedFeaturePolicy container_policy;
  if (GetFrame()->Owner())
    container_policy = GetFrame()->Owner()->ContainerPolicy();
  GetFrame()->GetSecurityContext()->InitializeFeaturePolicy(
      feature_policy_header_, container_policy, parent_feature_policy);
}

void WebRemoteFrameImpl::AddReplicatedContentSecurityPolicyHeader(
    const WebString& header_value,
    WebContentSecurityPolicyType type,
    WebContentSecurityPolicySource source) {
  GetFrame()
      ->GetSecurityContext()
      ->GetContentSecurityPolicy()
      ->AddPolicyFromHeaderValue(
          header_value, static_cast<ContentSecurityPolicyHeaderType>(type),
          static_cast<ContentSecurityPolicyHeaderSource>(source));
}

void WebRemoteFrameImpl::ResetReplicatedContentSecurityPolicy() {
  GetFrame()->GetSecurityContext()->ResetReplicatedContentSecurityPolicy();
}

void WebRemoteFrameImpl::SetReplicatedInsecureRequestPolicy(
    WebInsecureRequestPolicy policy) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->SetInsecureRequestPolicy(policy);
}

void WebRemoteFrameImpl::SetReplicatedInsecureNavigationsSet(
    const std::vector<unsigned>& set) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->SetInsecureNavigationsSet(set);
}

void WebRemoteFrameImpl::ForwardResourceTimingToParent(
    const WebResourceTimingInfo& info) {
  DCHECK(Parent()->IsWebLocalFrame());
  WebLocalFrameImpl* parent_frame =
      ToWebLocalFrameImpl(Parent()->ToWebLocalFrame());
  HTMLFrameOwnerElement* owner_element =
      ToHTMLFrameOwnerElement(frame_->Owner());
  DOMWindowPerformance::performance(*parent_frame->GetFrame()->DomWindow())
      ->AddResourceTiming(info, owner_element->localName());
}

void WebRemoteFrameImpl::DispatchLoadEventForFrameOwner() {
  DCHECK(GetFrame()->Owner()->IsLocal());
  GetFrame()->Owner()->DispatchLoad();
}

void WebRemoteFrameImpl::DidStartLoading() {
  GetFrame()->SetIsLoading(true);
}

void WebRemoteFrameImpl::DidStopLoading() {
  GetFrame()->SetIsLoading(false);
}

bool WebRemoteFrameImpl::IsIgnoredForHitTest() const {
  return GetFrame()->IsIgnoredForHitTest();
}

void WebRemoteFrameImpl::WillEnterFullscreen() {
  // This should only ever be called when the FrameOwner is local.
  HTMLFrameOwnerElement* owner_element =
      ToHTMLFrameOwnerElement(GetFrame()->Owner());

  // Call |requestFullscreen()| on |ownerElement| to make it the pending
  // fullscreen element in anticipation of the coming |didEnterFullscreen()|
  // call.
  //
  // PrefixedForCrossProcessDescendant is necessary because:
  //  - The fullscreen element ready check and other checks should be bypassed.
  //  - |ownerElement| will need :-webkit-full-screen-ancestor style in addition
  //    to :-webkit-full-screen.
  //
  // TODO(alexmos): currently, this assumes prefixed requests, but in the
  // future, this should plumb in information about which request type
  // (prefixed or unprefixed) to use for firing fullscreen events.
  Fullscreen::RequestFullscreen(
      *owner_element, FullscreenOptions(),
      Fullscreen::RequestType::kPrefixedForCrossProcessDescendant);
}

void WebRemoteFrameImpl::UpdateUserActivationState(
    UserActivationUpdateType update_type) {
  switch (update_type) {
    case UserActivationUpdateType::kNotifyActivation:
      GetFrame()->NotifyUserActivationInLocalTree();
      break;
    case UserActivationUpdateType::kConsumeTransientActivation:
      GetFrame()->ConsumeTransientUserActivationInLocalTree();
      break;
  }
}

void WebRemoteFrameImpl::ScrollRectToVisible(
    const WebRect& rect_to_scroll,
    const WebScrollIntoViewParams& params) {
  Element* owner_element = frame_->DeprecatedLocalOwner();
  LayoutObject* owner_object = owner_element->GetLayoutObject();
  if (!owner_object) {
    // The LayoutObject could be nullptr by the time we get here. For instance
    // <iframe>'s style might have been set to 'display: none' right after
    // scrolling starts in the OOPIF's process (see https://crbug.com/777811).
    return;
  }

  // Schedule the scroll.
  LayoutRect absolute_rect = EnclosingLayoutRect(
      owner_object
          ->LocalToAncestorQuad(FloatRect(rect_to_scroll), owner_object->View(),
                                kUseTransforms)
          .BoundingBox());

  if (!params.zoom_into_rect ||
      !owner_object->GetDocument().GetFrame()->LocalFrameRoot().IsMainFrame()) {
    owner_object->ScrollRectToVisible(absolute_rect, params);
    return;
  }

  // ZoomAndScrollToFocusedEditableElementRect will scroll only the layout and
  // visual viewports. Ensure the element is actually visible in the viewport
  // scrolling layer. (i.e. isn't clipped by some other content).
  WebScrollIntoViewParams new_params(params);
  new_params.stop_at_main_frame_layout_viewport = true;
  absolute_rect = owner_object->ScrollRectToVisible(absolute_rect, new_params);

  // This is due to something such as scroll focused editable element into
  // view on Android which also requires an automatic zoom into legible scale.
  // This is handled by main frame's WebView.
  WebViewImpl* view_impl = static_cast<WebViewImpl*>(View());
  IntRect rect_in_document =
      view_impl->MainFrameImpl()->GetFrame()->View()->RootFrameToDocument(
          EnclosingIntRect(
              owner_element->GetDocument().View()->ConvertToRootFrame(
                  absolute_rect)));
  IntRect element_bounds_in_document = EnclosingIntRect(
      DeNormalizeRect(params.relative_element_bounds, rect_in_document));
  IntRect caret_bounds_in_document = EnclosingIntRect(
      DeNormalizeRect(params.relative_caret_bounds, rect_in_document));
  view_impl->ZoomAndScrollToFocusedEditableElementRect(
      element_bounds_in_document, caret_bounds_in_document, true);
}

void WebRemoteFrameImpl::BubbleLogicalScroll(WebScrollDirection direction,
                                             WebScrollGranularity granularity) {
  Frame* parent_frame = GetFrame()->Tree().Parent();
  DCHECK(parent_frame);
  DCHECK(parent_frame->IsLocalFrame());

  parent_frame->BubbleLogicalScrollFromChildFrame(direction, granularity,
                                                  GetFrame());
}

void WebRemoteFrameImpl::IntrinsicSizingInfoChanged(
    const WebIntrinsicSizingInfo& web_sizing_info) {
  FrameOwner* owner = GetFrame()->Owner();
  // Only communication from HTMLPluginElement-owned subframes is allowed
  // at present. This includes <embed> and <object> tags.
  if (!owner || !owner->IsPlugin())
    return;

  IntrinsicSizingInfo sizing_info;
  sizing_info.size = web_sizing_info.size;
  sizing_info.aspect_ratio = web_sizing_info.aspect_ratio;
  sizing_info.has_width = web_sizing_info.has_width;
  sizing_info.has_height = web_sizing_info.has_height;
  frame_->View()->SetIntrinsicSizeInfo(sizing_info);

  owner->IntrinsicSizingInfoChanged();
}

void WebRemoteFrameImpl::SetHasReceivedUserGestureBeforeNavigation(bool value) {
  GetFrame()->SetDocumentHasReceivedUserGestureBeforeNavigation(value);
}

v8::Local<v8::Object> WebRemoteFrameImpl::GlobalProxy() const {
  return GetFrame()
      ->GetWindowProxy(DOMWrapperWorld::MainWorld())
      ->GlobalProxyIfNotDetached();
}

WebRect WebRemoteFrameImpl::GetCompositingRect() {
  return GetFrame()->View()->GetCompositingRect();
}

void WebRemoteFrameImpl::RenderFallbackContent() const {
  // TODO(ekaramad): If the owner renders its own content, then the current
  // ContentFrame() should detach and free-up the OOPIF process (see
  // https://crbug.com/850223).
  auto* owner = frame_->DeprecatedLocalOwner();
  DCHECK(IsHTMLObjectElement(owner));
  owner->RenderFallbackContent(frame_);
}

WebRemoteFrameImpl::WebRemoteFrameImpl(WebTreeScopeType scope,
                                       WebRemoteFrameClient* client)
    : WebRemoteFrame(scope),
      client_(client),
      frame_client_(RemoteFrameClientImpl::Create(this)),
      self_keep_alive_(this) {
  DCHECK(client);
}

}  // namespace blink
