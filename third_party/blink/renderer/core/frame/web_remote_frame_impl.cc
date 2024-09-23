// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
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
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/quad_f.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
mojom::blink::FrameReplicationStatePtr ToBlinkFrameReplicationState(
    mojom::FrameReplicationStatePtr to_convert) {
  mojom::blink::FrameReplicationStatePtr result =
      mojom::blink::FrameReplicationState::New();
  result->origin = SecurityOrigin::CreateFromUrlOrigin(to_convert->origin);
  result->name = WebString::FromUTF8(to_convert->name);
  result->unique_name = WebString::FromUTF8(to_convert->unique_name);

  for (const auto& header : to_convert->permissions_policy_header)
    result->permissions_policy_header.push_back(header);

  result->active_sandbox_flags = to_convert->active_sandbox_flags;
  result->frame_policy = to_convert->frame_policy;
  result->insecure_request_policy = to_convert->insecure_request_policy;

  for (const auto& value : to_convert->insecure_navigations_set)
    result->insecure_navigations_set.push_back(value);

  result->has_potentially_trustworthy_unique_origin =
      to_convert->has_potentially_trustworthy_unique_origin;
  result->has_active_user_gesture = to_convert->has_active_user_gesture;
  result->has_received_user_gesture_before_nav =
      to_convert->has_received_user_gesture_before_nav;
  result->is_ad_frame = to_convert->is_ad_frame;
  return result;
}

}  // namespace

WebRemoteFrame* WebRemoteFrame::FromFrameToken(
    const RemoteFrameToken& frame_token) {
  auto* frame = RemoteFrame::FromFrameToken(frame_token);
  if (!frame)
    return nullptr;
  return WebRemoteFrameImpl::FromFrame(*frame);
}

WebRemoteFrame* WebRemoteFrame::Create(mojom::blink::TreeScopeType scope,
                                       const RemoteFrameToken& frame_token) {
  return MakeGarbageCollected<WebRemoteFrameImpl>(scope, frame_token);
}

// static
WebRemoteFrame* WebRemoteFrame::CreateMainFrame(
    WebView* web_view,
    const RemoteFrameToken& frame_token,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener,
    CrossVariantMojoAssociatedRemote<mojom::blink::RemoteFrameHostInterfaceBase>
        remote_frame_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::RemoteFrameInterfaceBase>
        receiver,
    mojom::FrameReplicationStatePtr replicated_state) {
  return WebRemoteFrameImpl::CreateMainFrame(
      web_view, frame_token, is_loading, devtools_frame_token, opener,
      std::move(remote_frame_host), std::move(receiver),
      ToBlinkFrameReplicationState(std::move(replicated_state)));
}

// static
WebRemoteFrameImpl* WebRemoteFrameImpl::CreateMainFrame(
    WebView* web_view,
    const RemoteFrameToken& frame_token,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver,
    mojom::blink::FrameReplicationStatePtr replicated_state) {
  WebRemoteFrameImpl* frame = MakeGarbageCollected<WebRemoteFrameImpl>(
      mojom::blink::TreeScopeType::kDocument, frame_token);
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
      devtools_frame_token, std::move(remote_frame_host), std::move(receiver));
  frame->SetReplicatedState(std::move(replicated_state));
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*frame)->SetOpenerDoNotNotify(opener_frame);
  if (is_loading) {
    frame->DidStartLoading();
  }
  return frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::CreateForFencedFrame(
    mojom::blink::TreeScopeType scope,
    const RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    HTMLFrameOwnerElement* frame_owner,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver,
    mojom::blink::FrameReplicationStatePtr replicated_state) {
  // We first convert this to a raw blink::Element*, and manually convert this
  // to an HTMLElement*. That is the only way the IsA<> and To<> casts below
  // will work.
  DCHECK(IsA<HTMLFencedFrameElement>(frame_owner));
  auto* frame = MakeGarbageCollected<WebRemoteFrameImpl>(scope, frame_token);
  ExecutionContext* execution_context = frame_owner->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(execution_context));
  LocalFrame* host_frame = frame_owner->GetDocument().GetFrame();
  frame->InitializeCoreFrame(
      *host_frame->GetPage(), frame_owner, /*parent=*/nullptr,
      /*previous_sibling=*/nullptr, FrameInsertType::kInsertInConstructor,
      g_null_atom, &host_frame->window_agent_factory(), devtools_frame_token,
      std::move(remote_frame_host), std::move(receiver));
  frame->SetReplicatedState(std::move(replicated_state));
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const WebLocalFrame* WebRemoteFrameImpl::ToWebLocalFrame() const {
  NOTREACHED_IN_MIGRATION();
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
    const DocumentToken& document_token,
    CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>
        interface_broker,
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

  // TODO(https://crbug.com/1355751): Plumb the StorageKey from a value provided
  // by the browser process. This was attempted in patchset 6 of:
  // https://chromium-review.googlesource.com/c/chromium/src/+/3851381/6
  // A remote frame being asked to create a child only happens in some cases to
  // recover from a crash.
  StorageKey storage_key;

  child->InitializeCoreFrame(
      *GetFrame()->GetPage(), owner, this, previous_sibling,
      FrameInsertType::kInsertInConstructor, name, window_agent_factory, opener,
      document_token, std::move(interface_broker), std::move(policy_container),
      storage_key,
      /*creator_base_url=*/KURL());
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
    const base::UnguessableToken& devtools_frame_token,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame>
        remote_frame_receiver) {
  Frame* parent_frame = parent ? ToCoreFrame(*parent) : nullptr;
  Frame* previous_sibling_frame =
      previous_sibling ? ToCoreFrame(*previous_sibling) : nullptr;

  // If this is not a top-level frame, we need to send FrameVisualProperties to
  // the remote renderer process. Some of the properties are inherited from the
  // WebFrameWidget containing this frame, and this is true for regular frames
  // in the frame tree as well as for fenced frames, which are not in the frame
  // tree; hence the code to traverse up through FrameOwner.
  WebFrameWidgetImpl* ancestor_widget = nullptr;
  if (parent) {
    if (parent->IsWebLocalFrame()) {
      ancestor_widget =
          To<WebLocalFrameImpl>(parent)->LocalRoot()->FrameWidgetImpl();
    }
  } else if (owner && owner->IsLocal()) {
    // Never gets to this point unless |owner| is a <fencedframe>
    // element.
    HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(owner);
    DCHECK(owner_element->IsHTMLFencedFrameElement());
    LocalFrame& local_frame =
        owner_element->GetDocument().GetFrame()->LocalFrameRoot();
    ancestor_widget =
        WebLocalFrameImpl::FromFrame(local_frame)->FrameWidgetImpl();
  }

  SetCoreFrame(MakeGarbageCollected<RemoteFrame>(
      frame_client_.Get(), page, owner, parent_frame, previous_sibling_frame,
      insert_type, GetRemoteFrameToken(), window_agent_factory, ancestor_widget,
      devtools_frame_token, std::move(remote_frame_host),
      std::move(remote_frame_receiver)));

  if (ancestor_widget)
    InitializeFrameVisualProperties(ancestor_widget, View());

  GetFrame()->CreateView();
  frame_->Tree().SetName(name);
}

WebRemoteFrameImpl* WebRemoteFrameImpl::CreateRemoteChild(
    mojom::blink::TreeScopeType scope,
    const RemoteFrameToken& frame_token,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    WebFrame* opener,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame> receiver,
    mojom::blink::FrameReplicationStatePtr replicated_state,
    mojom::blink::FrameOwnerPropertiesPtr owner_properties) {
  auto* child = MakeGarbageCollected<WebRemoteFrameImpl>(scope, frame_token);
  auto* owner = MakeGarbageCollected<RemoteFrameOwner>(
      replicated_state->frame_policy, WebFrameOwnerProperties());
  WindowAgentFactory* window_agent_factory = nullptr;
  if (opener) {
    window_agent_factory = &ToCoreFrame(*opener)->window_agent_factory();
  } else {
    window_agent_factory = &GetFrame()->window_agent_factory();
  }

  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, this, LastChild(),
                             FrameInsertType::kInsertInConstructor,
                             AtomicString(replicated_state->name),
                             window_agent_factory, devtools_frame_token,
                             std::move(remote_frame_host), std::move(receiver));
  child->SetReplicatedState(std::move(replicated_state));
  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;
  ToCoreFrame(*child)->SetOpenerDoNotNotify(opener_frame);

  if (is_loading) {
    child->DidStartLoading();
  }

  DCHECK(owner_properties);
  child->SetFrameOwnerProperties(std::move(owner_properties));

  return child;
}

void WebRemoteFrameImpl::SetCoreFrame(RemoteFrame* frame) {
  frame_ = frame;
}

void WebRemoteFrameImpl::InitializeFrameVisualProperties(
    WebFrameWidgetImpl* ancestor_widget,
    WebView* web_view) {
  FrameVisualProperties visual_properties;
  visual_properties.zoom_level = ancestor_widget->GetZoomLevel();
  visual_properties.css_zoom_factor = ancestor_widget->GetCSSZoomFactor();
  visual_properties.page_scale_factor = ancestor_widget->PageScaleInMainFrame();
  visual_properties.is_pinch_gesture_active =
      ancestor_widget->PinchGestureActiveInMainFrame();
  visual_properties.screen_infos = ancestor_widget->GetOriginalScreenInfos();
  visual_properties.visible_viewport_size =
      ancestor_widget->VisibleViewportSizeInDIPs();
  const WebVector<gfx::Rect>& viewport_segments =
      ancestor_widget->ViewportSegments();
  visual_properties.root_widget_viewport_segments.assign(
      viewport_segments.begin(), viewport_segments.end());
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

void WebRemoteFrameImpl::DidStartLoading() {
  GetFrame()->DidStartLoading();
}

void WebRemoteFrameImpl::SetFrameOwnerProperties(
    mojom::blink::FrameOwnerPropertiesPtr owner_properties) {
  GetFrame()->SetFrameOwnerProperties(std::move(owner_properties));
}

v8::Local<v8::Object> WebRemoteFrameImpl::GlobalProxy(
    v8::Isolate* isolate) const {
  return GetFrame()
      ->GetWindowProxy(DOMWrapperWorld::MainWorld(isolate))
      ->GlobalProxyIfNotDetached()
      .ToLocalChecked();
}

gfx::Rect WebRemoteFrameImpl::GetCompositingRect() {
  return GetFrame()->View()->GetCompositingRect();
}

WebString WebRemoteFrameImpl::UniqueName() const {
  return GetFrame()->UniqueName();
}

const FrameVisualProperties&
WebRemoteFrameImpl::GetPendingVisualPropertiesForTesting() const {
  return GetFrame()->GetPendingVisualPropertiesForTesting();
}

bool WebRemoteFrameImpl::IsAdFrame() const {
  return GetFrame()->IsAdFrame();
}

WebRemoteFrameImpl::WebRemoteFrameImpl(mojom::blink::TreeScopeType scope,
                                       const RemoteFrameToken& frame_token)
    : WebRemoteFrame(scope, frame_token),
      frame_client_(MakeGarbageCollected<RemoteFrameClientImpl>(this)) {}

void WebRemoteFrameImpl::SetReplicatedState(
    mojom::FrameReplicationStatePtr replicated_state) {
  SetReplicatedState(ToBlinkFrameReplicationState(std::move(replicated_state)));
}

void WebRemoteFrameImpl::SetReplicatedState(
    mojom::blink::FrameReplicationStatePtr state) {
  RemoteFrame* remote_frame = GetFrame();
  DCHECK(remote_frame);

  remote_frame->SetReplicatedOrigin(
      state->origin, state->has_potentially_trustworthy_unique_origin);

#if DCHECK_IS_ON()
  scoped_refptr<const SecurityOrigin> security_origin_before_sandbox_flags =
      remote_frame->GetSecurityContext()->GetSecurityOrigin();
#endif

  remote_frame->DidSetFramePolicyHeaders(state->active_sandbox_flags,
                                         state->permissions_policy_header);

#if DCHECK_IS_ON()
  // If |state->has_potentially_trustworthy_unique_origin| is set,
  // - |state->origin| should be unique (this is checked in
  //   blink::SecurityOrigin::SetUniqueOriginIsPotentiallyTrustworthy() in
  //   SetReplicatedOrigin()), and thus
  // - The security origin is not updated by SetReplicatedSandboxFlags() and
  //   thus we don't have to apply |has_potentially_trustworthy_unique_origin|
  //   flag after SetReplicatedSandboxFlags().
  if (state->has_potentially_trustworthy_unique_origin) {
    DCHECK(security_origin_before_sandbox_flags ==
           remote_frame->GetSecurityContext()->GetSecurityOrigin());
  }
#endif

  remote_frame->SetReplicatedName(state->name, state->unique_name);
  remote_frame->SetInsecureRequestPolicy(state->insecure_request_policy);
  remote_frame->EnforceInsecureNavigationsSet(state->insecure_navigations_set);
  remote_frame->SetReplicatedIsAdFrame(state->is_ad_frame);

  if (state->has_active_user_gesture) {
    // TODO(crbug.com/1087963): This should be hearing about sticky activations
    // and setting those (as well as the active one?). But the call to
    // UpdateUserActivationState sets the transient activation.
    remote_frame->UpdateUserActivationState(
        mojom::UserActivationUpdateType::kNotifyActivation,
        mojom::UserActivationNotificationType::kMedia);
  }
  remote_frame->SetHadStickyUserActivationBeforeNavigation(
      state->has_received_user_gesture_before_nav);
}

}  // namespace blink
