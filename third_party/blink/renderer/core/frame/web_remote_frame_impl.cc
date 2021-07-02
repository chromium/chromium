// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"

#include <utility>

#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/execution_context/remote_security_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client_impl.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8.h"

namespace blink {

WebRemoteFrame* WebRemoteFrame::Create(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token) {
  return MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);
}

// static
WebRemoteFrame* WebRemoteFrame::CreateMainFrame(
    WebView* web_view,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener) {
  return WebRemoteFrameImpl::CreateMainFrame(
      web_view, client, interface_registry, associated_interface_provider,
      frame_token, devtools_frame_token, opener);
}

// static
WebRemoteFrame* WebRemoteFrame::CreateForPortalOrFencedFrame(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const WebElement& frame_owner) {
  return WebRemoteFrameImpl::CreateForPortalOrFencedFrame(
      scope, client, interface_registry, associated_interface_provider,
      frame_token, devtools_frame_token, frame_owner);
}

// static
WebRemoteFrameImpl* WebRemoteFrameImpl::CreateMainFrame(
    WebView* web_view,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener) {
  WebRemoteFrameImpl* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      mojom::blink::TreeScopeType::kDocument, client, interface_registry,
      associated_interface_provider, frame_token);
  Page& page = *To<WebViewImpl>(web_view)->GetPage();
  // It would be nice to DCHECK that the main frame is not set yet here.
  // Unfortunately, there is an edge case with a pending RenderFrameHost that
  // violates this: the embedder may create a pending RenderFrameHost for
  // navigating to a new page in a popup. If the navigation ends up redirecting
  // to a site that requires a process swap, it doesn't go through the standard
  // swapping path and instead directly overwrites the main frame.
  // TODO(dcheng): Remove the need for this and strongly enforce this condition
  // with a DCHECK.
  frame->InitializeCoreFrame(
      page, nullptr, nullptr, nullptr, FrameInsertType::kInsertInConstructor,
      g_null_atom,
      opener ? &ToCoreFrame(*opener)->window_agent_factory() : nullptr,
      devtools_frame_token);
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*frame)->SetOpenerDoNotNotify(opener_frame);
  return frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::CreateForPortalOrFencedFrame(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const WebElement& frame_owner) {
  auto* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);

  // We first convert this to a raw blink::Element*, and manually convert this
  // to an HTMLElement*. That is the only way the IsA<> and To<> casts below
  // will work.
  Element* element = frame_owner;
  DCHECK(IsA<HTMLPortalElement>(element) ||
         IsA<HTMLFencedFrameElement>(element));
  ExecutionContext* execution_context = element->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::PortalsEnabled(execution_context) ||
         RuntimeEnabledFeatures::FencedFramesEnabled(execution_context));
  HTMLFrameOwnerElement* frame_owner_element =
      To<HTMLFrameOwnerElement>(element);
  LocalFrame* host_frame = frame_owner_element->GetDocument().GetFrame();
  frame->InitializeCoreFrame(
      *host_frame->GetPage(), frame_owner_element, nullptr, nullptr,
      FrameInsertType::kInsertInConstructor, g_null_atom,
      &host_frame->window_agent_factory(), devtools_frame_token);

  return frame;
}

WebRemoteFrameImpl::~WebRemoteFrameImpl() = default;

void WebRemoteFrameImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_client_);
  visitor->Trace(frame_);
}

bool WebRemoteFrameImpl::IsWebLocalFrame() const {
  return false;
}

WebLocalFrame* WebRemoteFrameImpl::ToWebLocalFrame() {
  NOTREACHED();
  return nullptr;
}

const WebLocalFrame* WebRemoteFrameImpl::ToWebLocalFrame() const {
  NOTREACHED();
  return nullptr;
}

bool WebRemoteFrameImpl::IsWebRemoteFrame() const {
  return true;
}

WebRemoteFrame* WebRemoteFrameImpl::ToWebRemoteFrame() {
  return this;
}

const WebRemoteFrame* WebRemoteFrameImpl::ToWebRemoteFrame() const {
  return this;
}

void WebRemoteFrameImpl::Close() {
  WebRemoteFrame::Close();

  self_keep_alive_.Clear();
}

WebView* WebRemoteFrameImpl::View() const {
  if (!GetFrame()) {
    return nullptr;
  }
  DCHECK(GetFrame()->GetPage());
  return GetFrame()->GetPage()->GetChromeClient().GetWebView();
}

WebLocalFrame* WebRemoteFrameImpl::CreateLocalChild(
    mojom::blink::TreeScopeType scope,
    const WebString& name,
    const FramePolicy& frame_policy,
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    WebFrame* previous_sibling,
    const WebFrameOwnerProperties& frame_owner_properties,
    const LocalFrameToken& frame_token,
    WebFrame* opener,
    std::unique_ptr<WebPolicyContainer> policy_container) {
  auto* child = MakeGarbageCollected<WebLocalFrameImpl>(
      base::PassKey<WebRemoteFrameImpl>(), scope, client, interface_registry,
      frame_token);
  auto* owner = MakeGarbageCollected<RemoteFrameOwner>(frame_policy,
                                                       frame_owner_properties);

  WindowAgentFactory* window_agent_factory = nullptr;
  if (opener) {
    window_agent_factory = &ToCoreFrame(*opener)->window_agent_factory();
  } else {
    window_agent_factory = &GetFrame()->window_agent_factory();
  }

  child->InitializeCoreFrame(
      *GetFrame()->GetPage(), owner, this, previous_sibling,
      FrameInsertType::kInsertInConstructor, name, window_agent_factory, opener,
      std::move(policy_container));
  DCHECK(child->GetFrame());
  return child;
}

void WebRemoteFrameImpl::InitializeCoreFrame(
    Page& page,
    FrameOwner* owner,
    WebFrame* parent,
    WebFrame* previous_sibling,
    FrameInsertType insert_type,
    const AtomicString& name,
    WindowAgentFactory* window_agent_factory,
    const base::UnguessableToken& devtools_frame_token) {
  Frame* parent_frame = parent ? ToCoreFrame(*parent) : nullptr;
  Frame* previous_sibling_frame =
      previous_sibling ? ToCoreFrame(*previous_sibling) : nullptr;

  // If this is not a top-level frame, we need to send FrameVisualProperties to
  // the remote renderer process. Some of the properties are inherited from the
  // WebFrameWidget containing this frame, and this is true for regular frames
  // in the frame tree as well as for portals, which are not in the frame tree;
  // hence the code to traverse up through FrameOwner.
  WebFrameWidget* ancestor_widget = nullptr;
  if (parent) {
    if (parent->IsWebLocalFrame()) {
      ancestor_widget =
          To<WebLocalFrameImpl>(parent)->LocalRoot()->FrameWidget();
    }
  } else if (owner && owner->IsLocal()) {
    // Never gets to this point unless |owner| is a <portal> or <fencedframe>
    // element.
    HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(owner);
    DCHECK(owner_element->IsHTMLPortalElement() ||
           owner_element->IsHTMLFencedFrameElement());
    LocalFrame& local_frame =
        owner_element->GetDocument().GetFrame()->LocalFrameRoot();
    ancestor_widget = WebLocalFrameImpl::FromFrame(local_frame)->FrameWidget();
  }

  SetCoreFrame(MakeGarbageCollected<RemoteFrame>(
      frame_client_.Get(), page, owner, parent_frame, previous_sibling_frame,
      insert_type, GetRemoteFrameToken(), window_agent_factory,
      interface_registry_, associated_interface_provider_, ancestor_widget,
      devtools_frame_token));

  if (ancestor_widget)
    InitializeFrameVisualProperties(ancestor_widget, View());

  GetFrame()->CreateView();
  frame_->Tree().SetName(name);
}

WebRemoteFrame* WebRemoteFrameImpl::CreateRemoteChild(
    mojom::blink::TreeScopeType scope,
    const WebString& name,
    const FramePolicy& frame_policy,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener) {
  auto* child = MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);
  auto* owner = MakeGarbageCollected<RemoteFrameOwner>(
      frame_policy, WebFrameOwnerProperties());
  WindowAgentFactory* window_agent_factory = nullptr;
  if (opener) {
    window_agent_factory = &ToCoreFrame(*opener)->window_agent_factory();
  } else {
    window_agent_factory = &GetFrame()->window_agent_factory();
  }

  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, this, LastChild(),
                             FrameInsertType::kInsertInConstructor, name,
                             window_agent_factory, devtools_frame_token);
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*child)->SetOpenerDoNotNotify(opener_frame);
  return child;
}

void WebRemoteFrameImpl::SetCoreFrame(RemoteFrame* frame) {
  frame_ = frame;
}

void WebRemoteFrameImpl::InitializeFrameVisualProperties(
    WebFrameWidget* ancestor_widget,
    WebView* web_view) {
  FrameVisualProperties visual_properties;
  visual_properties.zoom_level = web_view->ZoomLevel();
  visual_properties.page_scale_factor = ancestor_widget->PageScaleInMainFrame();
  visual_properties.is_pinch_gesture_active =
      ancestor_widget->PinchGestureActiveInMainFrame();
  visual_properties.screen_info = ancestor_widget->GetOriginalScreenInfo();
  visual_properties.visible_viewport_size =
      ancestor_widget->VisibleViewportSizeInDIPs();
  const WebVector<gfx::Rect>& window_segments =
      ancestor_widget->WindowSegments();
  visual_properties.root_widget_window_segments.assign(window_segments.begin(),
                                                       window_segments.end());
  GetFrame()->InitializeFrameVisualProperties(visual_properties);
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
  GetFrame()->SetReplicatedOrigin(origin,
                                  is_potentially_trustworthy_opaque_origin);
}

void WebRemoteFrameImpl::SetReplicatedSandboxFlags(
    network::mojom::blink::WebSandboxFlags flags) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedSandboxFlags(flags);
}

void WebRemoteFrameImpl::SetReplicatedName(const WebString& name,
                                           const WebString& unique_name) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedName(name, unique_name);
}

void WebRemoteFrameImpl::SetReplicatedPermissionsPolicyHeader(
    const ParsedPermissionsPolicy& parsed_header) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedPermissionsPolicyHeader(parsed_header);
}

void WebRemoteFrameImpl::SetReplicatedInsecureRequestPolicy(
    mojom::blink::InsecureRequestPolicy policy) {
  DCHECK(GetFrame());
  GetFrame()->SetInsecureRequestPolicy(policy);
}

void WebRemoteFrameImpl::SetReplicatedInsecureNavigationsSet(
    const WebVector<unsigned>& set) {
  DCHECK(GetFrame());
  GetFrame()->SetInsecureNavigationsSet(set);
}

void WebRemoteFrameImpl::SetReplicatedIsAdSubframe(bool is_ad_subframe) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedIsAdSubframe(is_ad_subframe);
}

void WebRemoteFrameImpl::DidStartLoading() {
  GetFrame()->DidStartLoading();
}

bool WebRemoteFrameImpl::IsIgnoredForHitTest() const {
  return GetFrame()->IsIgnoredForHitTest();
}

void WebRemoteFrameImpl::UpdateUserActivationState(
    mojom::blink::UserActivationUpdateType update_type,
    mojom::blink::UserActivationNotificationType notification_type) {
  GetFrame()->UpdateUserActivationState(update_type, notification_type);
}

void WebRemoteFrameImpl::SetHadStickyUserActivationBeforeNavigation(
    bool value) {
  GetFrame()->SetHadStickyUserActivationBeforeNavigation(value);
}

v8::Local<v8::Object> WebRemoteFrameImpl::GlobalProxy() const {
  return GetFrame()
      ->GetWindowProxy(DOMWrapperWorld::MainWorld())
      ->GlobalProxyIfNotDetached();
}

gfx::Rect WebRemoteFrameImpl::GetCompositingRect() {
  return GetFrame()->View()->GetCompositingRect();
}

void WebRemoteFrameImpl::SynchronizeVisualProperties() {
  GetFrame()->SynchronizeVisualProperties();
}

void WebRemoteFrameImpl::ResendVisualProperties() {
  GetFrame()->ResendVisualProperties();
}

float WebRemoteFrameImpl::GetCompositingScaleFactor() {
  return GetFrame()->View()->GetCompositingScaleFactor();
}

WebString WebRemoteFrameImpl::UniqueName() const {
  return GetFrame()->UniqueName();
}

const FrameVisualProperties&
WebRemoteFrameImpl::GetPendingVisualPropertiesForTesting() const {
  return GetFrame()->GetPendingVisualPropertiesForTesting();
}

bool WebRemoteFrameImpl::IsAdSubframe() const {
  return GetFrame()->IsAdSubframe();
}

WebRemoteFrameImpl::WebRemoteFrameImpl(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const RemoteFrameToken& frame_token)
    : WebRemoteFrame(scope, frame_token),
      client_(client),
      frame_client_(MakeGarbageCollected<RemoteFrameClientImpl>(this)),
      interface_registry_(interface_registry),
      associated_interface_provider_(associated_interface_provider),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  DCHECK(client);
}

}  // namespace blink
