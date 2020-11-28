// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"

#include <utility>

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_range.h"
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
    const base::UnguessableToken& frame_token) {
  return MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);
}

WebRemoteFrame* WebRemoteFrame::CreateMainFrame(
    WebView* web_view,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token,
    WebFrame* opener) {
  return WebRemoteFrameImpl::CreateMainFrame(
      web_view, client, interface_registry, associated_interface_provider,
      frame_token, opener);
}

WebRemoteFrame* WebRemoteFrame::CreateForPortal(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token,
    const WebElement& portal_element) {
  return WebRemoteFrameImpl::CreateForPortal(scope, client, interface_registry,
                                             associated_interface_provider,
                                             frame_token, portal_element);
}

// static
WebRemoteFrameImpl* WebRemoteFrameImpl::CreateMainFrame(
    WebView* web_view,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token,
    WebFrame* opener) {
  WebRemoteFrameImpl* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      mojom::blink::TreeScopeType::kDocument, client, interface_registry,
      associated_interface_provider, frame_token);
  Page& page = *static_cast<WebViewImpl*>(web_view)->GetPage();
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
      opener ? &ToCoreFrame(*opener)->window_agent_factory() : nullptr);
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*frame)->SetOpenerDoNotNotify(opener_frame);
  return frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::CreateForPortal(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token,
    const WebElement& portal_element) {
  auto* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);

  Element* element = portal_element;
  DCHECK(element->HasTagName(html_names::kPortalTag));
  DCHECK(
      RuntimeEnabledFeatures::PortalsEnabled(element->GetExecutionContext()));
  HTMLPortalElement* portal = static_cast<HTMLPortalElement*>(element);
  LocalFrame* host_frame = portal->GetDocument().GetFrame();
  frame->InitializeCoreFrame(*host_frame->GetPage(), portal, nullptr, nullptr,
                             FrameInsertType::kInsertInConstructor, g_null_atom,
                             &host_frame->window_agent_factory());

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
    blink::InterfaceRegistry* interface_registry,
    WebFrame* previous_sibling,
    const WebFrameOwnerProperties& frame_owner_properties,
    mojom::blink::FrameOwnerElementType frame_owner_element_type,
    const base::UnguessableToken& frame_token,
    WebFrame* opener,
    std::unique_ptr<blink::WebPolicyContainer> policy_container) {
  auto* child = MakeGarbageCollected<WebLocalFrameImpl>(
      base::PassKey<WebRemoteFrameImpl>(), scope, client, interface_registry,
      frame_token);
  auto* owner = MakeGarbageCollected<RemoteFrameOwner>(
      frame_policy, frame_owner_properties, frame_owner_element_type);

  WindowAgentFactory* window_agent_factory = nullptr;
  if (opener) {
    window_agent_factory = &ToCoreFrame(*opener)->window_agent_factory();
  } else if (!frame_policy.disallow_document_access) {
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
    WindowAgentFactory* window_agent_factory) {
  Frame* parent_frame = parent ? ToCoreFrame(*parent) : nullptr;
  Frame* previous_sibling_frame =
      previous_sibling ? ToCoreFrame(*previous_sibling) : nullptr;
  SetCoreFrame(MakeGarbageCollected<RemoteFrame>(
      frame_client_.Get(), page, owner, parent_frame, previous_sibling_frame,
      insert_type, GetFrameToken(), window_agent_factory, interface_registry_,
      associated_interface_provider_));
  GetFrame()->CreateView();
  frame_->Tree().SetName(name);
}

WebRemoteFrame* WebRemoteFrameImpl::CreateRemoteChild(
    mojom::blink::TreeScopeType scope,
    const WebString& name,
    const FramePolicy& frame_policy,
    mojom::blink::FrameOwnerElementType frame_owner_element_type,
    WebRemoteFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token,
    WebFrame* opener) {
  auto* child = MakeGarbageCollected<WebRemoteFrameImpl>(
      scope, client, interface_registry, associated_interface_provider,
      frame_token);
  auto* owner = MakeGarbageCollected<RemoteFrameOwner>(
      frame_policy, WebFrameOwnerProperties(), frame_owner_element_type);
  WindowAgentFactory* window_agent_factory = nullptr;
  if (opener) {
    window_agent_factory = &ToCoreFrame(*opener)->window_agent_factory();
  } else if (!frame_policy.disallow_document_access) {
    window_agent_factory = &GetFrame()->window_agent_factory();
  }

  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, this, LastChild(),
                             FrameInsertType::kInsertInConstructor, name,
                             window_agent_factory);
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*child)->SetOpenerDoNotNotify(opener_frame);
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

void WebRemoteFrameImpl::SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
    const ParsedFeaturePolicy& parsed_header,
    const FeaturePolicyFeatureState& opener_feature_state) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedFeaturePolicyHeaderAndOpenerPolicies(
      parsed_header, opener_feature_state);
}

void WebRemoteFrameImpl::AddReplicatedContentSecurityPolicyHeader(
    const WebString& header_value,
    network::mojom::ContentSecurityPolicyType type,
    network::mojom::ContentSecurityPolicySource source) {
  GetFrame()
      ->GetSecurityContext()
      ->GetContentSecurityPolicy()
      ->AddPolicyFromHeaderValue(header_value, type, source);
}

void WebRemoteFrameImpl::ResetReplicatedContentSecurityPolicy() {
  GetFrame()->ResetReplicatedContentSecurityPolicy();
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

void WebRemoteFrameImpl::SetReplicatedAdFrameType(
    mojom::blink::AdFrameType ad_frame_type) {
  DCHECK(GetFrame());
  GetFrame()->SetReplicatedAdFrameType(ad_frame_type);
}

void WebRemoteFrameImpl::SetVisualProperties(
    const blink::FrameVisualProperties& properties) {
  GetFrame()->SetVisualProperties(properties);
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

WebRect WebRemoteFrameImpl::GetCompositingRect() {
  return GetFrame()->View()->GetCompositingRect();
}

WebString WebRemoteFrameImpl::UniqueName() const {
  return GetFrame()->UniqueName();
}

WebRemoteFrameImpl::WebRemoteFrameImpl(
    mojom::blink::TreeScopeType scope,
    WebRemoteFrameClient* client,
    InterfaceRegistry* interface_registry,
    AssociatedInterfaceProvider* associated_interface_provider,
    const base::UnguessableToken& frame_token)
    : WebRemoteFrame(scope, frame_token),
      client_(client),
      frame_client_(MakeGarbageCollected<RemoteFrameClientImpl>(this)),
      interface_registry_(interface_registry),
      associated_interface_provider_(associated_interface_provider),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  DCHECK(client);
}

}  // namespace blink
