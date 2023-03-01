/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/frame/frame.h"

#include <memory>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// static
Frame* Frame::ResolveFrame(const FrameToken& frame_token) {
  if (frame_token.Is<RemoteFrameToken>())
    return RemoteFrame::FromFrameToken(frame_token.GetAs<RemoteFrameToken>());
  DCHECK(frame_token.Is<LocalFrameToken>());
  return LocalFrame::FromFrameToken(frame_token.GetAs<LocalFrameToken>());
}

Frame::~Frame() {
  InstanceCounters::DecrementCounter(InstanceCounters::kFrameCounter);
  DCHECK(!owner_);
  DCHECK(IsDetached());
}

void Frame::Trace(Visitor* visitor) const {
  visitor->Trace(tree_node_);
  visitor->Trace(page_);
  visitor->Trace(owner_);
  visitor->Trace(window_proxy_manager_);
  visitor->Trace(dom_window_);
  visitor->Trace(client_);
  visitor->Trace(opener_);
  visitor->Trace(parent_);
  visitor->Trace(previous_sibling_);
  visitor->Trace(next_sibling_);
  visitor->Trace(first_child_);
  visitor->Trace(last_child_);
  visitor->Trace(provisional_frame_);
  visitor->Trace(navigation_rate_limiter_);
  visitor->Trace(window_agent_factory_);
  visitor->Trace(opened_frame_tracker_);
}

bool Frame::Detach(FrameDetachType type) {
  TRACE_EVENT0("blink", "Frame::Detach");
  DCHECK(client_);
  // Detach() can be re-entered, so this can't simply DCHECK(IsAttached()).
  DCHECK(!IsDetached());
  lifecycle_.AdvanceTo(FrameLifecycle::kDetaching);
  PageDismissalScope in_page_dismissal;

  if (!DetachImpl(type))
    return false;

  DCHECK(!IsDetached());
  DCHECK(client_);

  GetPage()->GetFocusController().FrameDetached(this);
  // FrameDetached() can fire JS event listeners, so `this` might have been
  // reentrantly detached.
  if (!client_)
    return false;

  DCHECK(!IsDetached());

  // TODO(dcheng): FocusController::FrameDetached() *should* fire JS events,
  // hence the above check for `client_` being null. However, when this was
  // previously placed before the `FrameDetached()` call, nothing crashes, which
  // is suspicious. Investigate if we really don't need to fire JS events--and
  // if we don't, move `forbid_scripts` up to be instantiated sooner and
  // simplify this code.
  ScriptForbiddenScope forbid_scripts;

  if (type == FrameDetachType::kRemove) {
    if (provisional_frame_) {
      provisional_frame_->Detach(FrameDetachType::kRemove);
    }
    SetOpener(nullptr);
    opened_frame_tracker_.Dispose();
    // Clearing the window proxies can call back into `LocalFrameClient`, so
    // this must be done before nulling out `client_` below.
    GetWindowProxyManager()->ClearForClose();
  } else {
    // In the case of a swap, detach is carefully coordinated with `Swap()`.
    // Intentionally avoid clearing the opener with `SetOpener(nullptr)` here,
    // since `Swap()` needs the original value to clone to the new frame.
    DCHECK_EQ(FrameDetachType::kSwap, type);

    // Clearing the window proxies can call back into `LocalFrameClient`, so
    // this must be done before nulling out `client_` below.
    // `ClearForSwap()` preserves the v8::Objects that represent the global
    // proxies; `Swap()` will later use `ReleaseGlobalProxies()` +
    // `SetGlobalProxies()` to adopt the global proxies into the new frame.
    GetWindowProxyManager()->ClearForSwap();
  }

  // After this, we must no longer talk to the client since this clears
  // its owning reference back to our owning LocalFrame.
  client_->Detached(type);
  client_ = nullptr;
  // Mark the frame as detached once |client_| is null, as most of the frame has
  // been torn down at this point.
  // TODO(dcheng): Once https://crbug.com/820782 is fixed, Frame::Client() will
  // also assert that it is only accessed when the frame is not detached.
  lifecycle_.AdvanceTo(FrameLifecycle::kDetached);
  // TODO(dcheng): This currently needs to happen after calling
  // FrameClient::Detached() to make it easier for FrameClient::Detached()
  // implementations to detect provisional frames and avoid removing them from
  // the frame tree. https://crbug.com/578349.
  DisconnectOwnerElement();
  page_ = nullptr;
  embedding_token_ = absl::nullopt;

  return true;
}

void Frame::DisconnectOwnerElement() {
  if (!owner_)
    return;

  // TODO(https://crbug.com/578349): If this is a provisional frame, the frame
  // owner doesn't actually point to this frame, so don't clear it. Note that
  // this can't use IsProvisional() because the |client_| is null already.
  if (owner_->ContentFrame() == this)
    owner_->ClearContentFrame();

  owner_ = nullptr;
}

Page* Frame::GetPage() const {
  return page_;
}

bool Frame::IsMainFrame() const {
  return !Tree().Parent();
}

bool Frame::IsOutermostMainFrame() const {
  return IsMainFrame() && !IsInFencedFrameTree();
}

bool Frame::IsCrossOriginToNearestMainFrame() const {
  DCHECK(GetSecurityContext());
  const SecurityOrigin* security_origin =
      GetSecurityContext()->GetSecurityOrigin();
  return !security_origin->CanAccess(
      Tree().Top().GetSecurityContext()->GetSecurityOrigin());
}

bool Frame::IsCrossOriginToOutermostMainFrame() const {
  return IsCrossOriginToNearestMainFrame() || IsInFencedFrameTree();
}

bool Frame::IsCrossOriginToParentOrOuterDocument() const {
  DCHECK(GetSecurityContext());
  if (IsInFencedFrameTree())
    return true;
  if (IsMainFrame())
    return false;
  Frame* parent = Tree().Parent();
  const SecurityOrigin* parent_security_origin =
      parent->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* security_origin =
      GetSecurityContext()->GetSecurityOrigin();
  return !security_origin->CanAccess(parent_security_origin);
}

HTMLFrameOwnerElement* Frame::DeprecatedLocalOwner() const {
  return DynamicTo<HTMLFrameOwnerElement>(owner_.Get());
}

static ChromeClient& GetEmptyChromeClient() {
  DEFINE_STATIC_LOCAL(Persistent<EmptyChromeClient>, client,
                      (MakeGarbageCollected<EmptyChromeClient>()));
  return *client;
}

ChromeClient& Frame::GetChromeClient() const {
  if (Page* page = GetPage())
    return page->GetChromeClient();
  return GetEmptyChromeClient();
}

Frame* Frame::FindUnsafeParentScrollPropagationBoundary() {
  Frame* current_frame = this;
  Frame* ancestor_frame = Tree().Parent();

  while (ancestor_frame) {
    if (!ancestor_frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            GetSecurityContext()->GetSecurityOrigin()))
      return current_frame;
    current_frame = ancestor_frame;
    ancestor_frame = ancestor_frame->Tree().Parent();
  }
  return nullptr;
}

LayoutEmbeddedContent* Frame::OwnerLayoutObject() const {
  if (!DeprecatedLocalOwner())
    return nullptr;
  return DeprecatedLocalOwner()->GetLayoutEmbeddedContent();
}

Settings* Frame::GetSettings() const {
  if (GetPage())
    return &GetPage()->GetSettings();
  return nullptr;
}

WindowProxy* Frame::GetWindowProxy(DOMWrapperWorld& world) {
  return window_proxy_manager_->GetWindowProxy(world);
}

WindowProxy* Frame::GetWindowProxyMaybeUninitialized(DOMWrapperWorld& world) {
  return window_proxy_manager_->GetWindowProxyMaybeUninitialized(world);
}

void Frame::DidChangeVisibilityState() {
  HeapVector<Member<Frame>> child_frames;
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    child_frames.push_back(child);
  for (wtf_size_t i = 0; i < child_frames.size(); ++i)
    child_frames[i]->DidChangeVisibilityState();
}

void Frame::NotifyUserActivationInFrameTree(
    mojom::blink::UserActivationNotificationType notification_type) {
  for (Frame* node = this; node; node = node->Tree().Parent()) {
    node->user_activation_state_.Activate(notification_type);
    node->ActivateHistoryUserActivationState();
  }

  // See the "Same-origin Visibility" section in |UserActivationState| class
  // doc.
  auto* local_frame = DynamicTo<LocalFrame>(this);
  if (local_frame &&
      RuntimeEnabledFeatures::UserActivationSameOriginVisibilityEnabled()) {
    const SecurityOrigin* security_origin =
        local_frame->GetSecurityContext()->GetSecurityOrigin();

    for (Frame* node = &Tree().Top(); node;
         node = node->Tree().TraverseNext()) {
      auto* local_frame_node = DynamicTo<LocalFrame>(node);
      if (local_frame_node &&
          security_origin->CanAccess(
              local_frame_node->GetSecurityContext()->GetSecurityOrigin())) {
        node->user_activation_state_.Activate(notification_type);
        node->ActivateHistoryUserActivationState();
      }
    }
  }
}

bool Frame::ConsumeTransientUserActivationInFrameTree() {
  bool was_active = user_activation_state_.IsActive();
  Frame& root = Tree().Top();

  // To record UMA once per consumption, we arbitrarily picked the LocalFrame
  // for root.
  if (IsA<LocalFrame>(root))
    root.user_activation_state_.RecordPreconsumptionUma();

  for (Frame* node = &root; node; node = node->Tree().TraverseNext())
    node->user_activation_state_.ConsumeIfActive();

  return was_active;
}

void Frame::ClearUserActivationInFrameTree() {
  for (Frame* node = this; node; node = node->Tree().TraverseNext(this)) {
    node->user_activation_state_.Clear();
    node->ClearHistoryUserActivationState();
  }
}

void Frame::RenderFallbackContent() {
  // Fallback has been requested by the browser navigation code, so triggering
  // the fallback content should also dispatch an error event.
  To<HTMLObjectElement>(Owner())->RenderFallbackContent(
      HTMLObjectElement::ErrorEventPolicy::kDispatch);
}

void Frame::RenderFallbackContentWithResourceTiming(
    mojom::blink::ResourceTimingInfoPtr timing,
    const String& server_timing_value) {
  auto* local_dom_window = To<LocalDOMWindow>(Parent()->DomWindow());
  DOMWindowPerformance::performance(*local_dom_window)
      ->AddResourceTimingWithUnparsedServerTiming(
          std::move(timing), server_timing_value,
          html_names::kObjectTag.LocalName());
  RenderFallbackContent();
}

bool Frame::IsInFencedFrameTree() const {
  DCHECK(!IsDetached());
  if (!features::IsFencedFramesEnabled())
    return false;

  return GetPage() && GetPage()->IsMainFrameFencedFrameRoot();
}

bool Frame::IsFencedFrameRoot() const {
  DCHECK(!IsDetached());
  if (!features::IsFencedFramesEnabled())
    return false;

  return IsInFencedFrameTree() && IsMainFrame();
}

absl::optional<mojom::blink::FencedFrameMode> Frame::GetFencedFrameMode()
    const {
  DCHECK(!IsDetached());

  if (!features::IsFencedFramesEnabled())
    return absl::nullopt;

  if (!IsInFencedFrameTree())
    return absl::nullopt;

  return GetPage()->FencedFrameMode();
}

void Frame::SetOwner(FrameOwner* owner) {
  owner_ = owner;
  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
}

void Frame::UpdateInertIfPossible() {
  if (auto* frame_owner_element =
          DynamicTo<HTMLFrameOwnerElement>(owner_.Get())) {
    const ComputedStyle* style = frame_owner_element->GetComputedStyle();
    const LocalFrame* parent = DynamicTo<LocalFrame>(Parent());
    SetIsInert((style && style->IsInert()) || (parent && parent->IsInert()));
  }
}

void Frame::UpdateInheritedEffectiveTouchActionIfPossible() {
  if (owner_) {
    Frame* owner_frame = owner_->ContentFrame();
    if (owner_frame) {
      SetInheritedEffectiveTouchAction(
          owner_frame->InheritedEffectiveTouchAction());
    }
  }
}

void Frame::UpdateVisibleToHitTesting() {
  bool parent_visible_to_hit_testing = true;
  if (auto* parent = Tree().Parent())
    parent_visible_to_hit_testing = parent->GetVisibleToHitTesting();

  bool self_visible_to_hit_testing = true;
  if (auto* local_owner = DynamicTo<HTMLFrameOwnerElement>(owner_.Get())) {
    self_visible_to_hit_testing =
        local_owner->GetLayoutObject()
            ? local_owner->GetLayoutObject()->Style()->VisibleToHitTesting()
            : true;
  }

  bool visible_to_hit_testing =
      parent_visible_to_hit_testing && self_visible_to_hit_testing;
  bool changed = visible_to_hit_testing_ != visible_to_hit_testing;
  visible_to_hit_testing_ = visible_to_hit_testing;
  if (changed)
    DidChangeVisibleToHitTesting();
}

const std::string& Frame::ToTraceValue() {
  // token's ToString() is latin1.
  if (!trace_value_)
    trace_value_ = devtools_frame_token_.ToString();
  return trace_value_.value();
}

void Frame::SetEmbeddingToken(const base::UnguessableToken& embedding_token) {
  embedding_token_ = embedding_token;
  if (auto* owner = DynamicTo<HTMLFrameOwnerElement>(Owner())) {
    // The embedding token is also used as the AXTreeID to reference the child
    // accessibility tree for an HTMLFrameOwnerElement, so we need to notify the
    // AXObjectCache object whenever this changes, to get the AX tree updated.
    if (AXObjectCache* cache = owner->GetDocument().ExistingAXObjectCache())
      cache->EmbeddingTokenChanged(owner);
  }
}

Frame::Frame(FrameClient* client,
             Page& page,
             FrameOwner* owner,
             Frame* parent,
             Frame* previous_sibling,
             FrameInsertType insert_type,
             const FrameToken& frame_token,
             const base::UnguessableToken& devtools_frame_token,
             WindowProxyManager* window_proxy_manager,
             WindowAgentFactory* inheriting_agent_factory)
    : tree_node_(this),
      page_(&page),
      owner_(owner),
      client_(client),
      window_proxy_manager_(window_proxy_manager),
      parent_(parent),
      navigation_rate_limiter_(*this),
      window_agent_factory_(inheriting_agent_factory
                                ? inheriting_agent_factory
                                : MakeGarbageCollected<WindowAgentFactory>(
                                      page.GetAgentGroupScheduler())),
      is_loading_(false),
      devtools_frame_token_(devtools_frame_token),
      frame_token_(frame_token) {
  InstanceCounters::IncrementCounter(InstanceCounters::kFrameCounter);
  if (parent_ && insert_type == FrameInsertType::kInsertInConstructor) {
    parent_->InsertAfter(this, previous_sibling);
  } else {
    CHECK(!previous_sibling);
  }
}

void Frame::Initialize() {
  // This frame must either be local or remote.
  DCHECK_NE(IsLocalFrame(), IsRemoteFrame());

  if (owner_)
    owner_->SetContentFrame(*this);
  else
    page_->SetMainFrame(this);
}

void Frame::FocusImpl() {
  // This uses FocusDocumentView rather than SetFocusedFrame so that blur
  // events are properly dispatched on any currently focused elements.
  // It is currently only used when replicating focus changes for
  // cross-process frames so |notify_embedder| is false to avoid sending
  // DidFocus updates from FocusController to the browser process,
  // which already knows the latest focused frame.
  GetPage()->GetFocusController().FocusDocumentView(
      this, false /* notify_embedder */);
}

void Frame::ApplyFrameOwnerProperties(
    mojom::blink::FrameOwnerPropertiesPtr properties) {
  // At the moment, this is only used to replicate frame owner properties
  // for frames with a remote owner.
  auto* owner = To<RemoteFrameOwner>(Owner());

  owner->SetBrowsingContextContainerName(properties->name);
  owner->SetScrollbarMode(properties->scrollbar_mode);
  owner->SetMarginWidth(properties->margin_width);
  owner->SetMarginHeight(properties->margin_height);
  owner->SetAllowFullscreen(properties->allow_fullscreen);
  owner->SetAllowPaymentRequest(properties->allow_payment_request);
  owner->SetIsDisplayNone(properties->is_display_none);
  owner->SetColorScheme(properties->color_scheme);
}

void Frame::InsertAfter(Frame* new_child, Frame* previous_sibling) {
  // Parent must match the one set in the constructor
  CHECK_EQ(new_child->parent_, this);

  Frame* next;
  if (!previous_sibling) {
    // Insert at the beginning if no previous sibling is specified.
    next = first_child_;
    first_child_ = new_child;
  } else {
    DCHECK_EQ(previous_sibling->parent_, this);
    next = previous_sibling->next_sibling_;
    previous_sibling->next_sibling_ = new_child;
    new_child->previous_sibling_ = previous_sibling;
  }

  if (next) {
    new_child->next_sibling_ = next;
    next->previous_sibling_ = new_child;
  } else {
    last_child_ = new_child;
  }

  Tree().InvalidateScopedChildCount();
  GetPage()->IncrementSubframeCount();
}

base::OnceClosure Frame::ScheduleFormSubmission(
    FrameScheduler* scheduler,
    FormSubmission* form_submission) {
  form_submit_navigation_task_ = PostCancellableTask(
      *scheduler->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      WTF::BindOnce(&FormSubmission::Navigate,
                    WrapPersistent(form_submission)));
  form_submit_navigation_task_version_++;

  return WTF::BindOnce(&Frame::CancelFormSubmissionWithVersion,
                       WrapWeakPersistent(this),
                       form_submit_navigation_task_version_);
}

void Frame::CancelFormSubmission() {
  form_submit_navigation_task_.Cancel();
}

void Frame::CancelFormSubmissionWithVersion(uint64_t version) {
  if (form_submit_navigation_task_version_ == version)
    form_submit_navigation_task_.Cancel();
}

bool Frame::IsFormSubmissionPending() {
  return form_submit_navigation_task_.IsActive();
}

void Frame::FocusPage(LocalFrame* originating_frame) {
  // We only allow focus to move to the |frame|'s page when the request comes
  // from a user gesture. (See https://bugs.webkit.org/show_bug.cgi?id=33389.)
  if (originating_frame &&
      LocalFrame::HasTransientUserActivation(originating_frame)) {
    // Ask the broswer process to focus the page.
    GetPage()->GetChromeClient().FocusPage();

    // Tattle on the frame that called |window.focus()|.
    originating_frame->GetLocalFrameHostRemote().DidCallFocus();
  }

  // Always report the attempt to focus the page to the Chrome client for
  // testing purposes (i.e. see WebViewTest.FocusExistingFrameOnNavigate()).
  GetPage()->GetChromeClient().DidFocusPage();
}

void Frame::SetOpenerDoNotNotify(Frame* opener) {
  if (opener_)
    opener_->opened_frame_tracker_.Remove(this);
  if (opener)
    opener->opened_frame_tracker_.Add(this);
  opener_ = opener;
}

Frame* Frame::Parent() const {
  // |parent_| will be null if detached, return early before accessing
  // Page.
  if (!parent_)
    return nullptr;

  return parent_;
}

Frame* Frame::Top() {
  Frame* parent = this;
  while (true) {
    Frame* next_parent = parent->Parent();
    if (!next_parent)
      break;
    parent = next_parent;
  }
  return parent;
}

bool Frame::AllowFocusWithoutUserActivation() {
  if (!features::IsFencedFramesEnabled())
    return true;

  if (!IsInFencedFrameTree())
    return true;

  // Inside a fenced frame tree, a frame can only request focus is its focus
  // controller already has focus.
  return GetPage()->GetFocusController().IsFocused();
}

bool Frame::Swap(WebLocalFrame* new_web_frame) {
  return SwapImpl(new_web_frame, mojo::NullAssociatedRemote(),
                  mojo::NullAssociatedReceiver());
}

bool Frame::Swap(WebRemoteFrame* new_web_frame,
                 mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
                     remote_frame_host,
                 mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame>
                     remote_frame_receiver) {
  return SwapImpl(new_web_frame, std::move(remote_frame_host),
                  std::move(remote_frame_receiver));
}

bool Frame::SwapImpl(
    WebFrame* new_web_frame,
    mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
        remote_frame_host,
    mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame>
        remote_frame_receiver) {
  DCHECK(IsAttached());

  using std::swap;

  // Important: do not cache frame tree pointers (e.g.  `previous_sibling_`,
  // `next_sibling_`, `first_child_`, `last_child_`) here. It is possible for
  // `Detach()` to mutate the frame tree and cause cached values to become
  // invalid.
  FrameOwner* owner = owner_;
  FrameSwapScope frame_swap_scope(owner);
  Page* page = page_;
  AtomicString name = Tree().GetName();

  // TODO(dcheng): This probably isn't necessary if we fix the ordering of
  // events in `Swap()`, e.g. `Detach()` should not happen before
  // `new_web_frame` is swapped in.
  // If there is a local parent, it might incorrectly declare itself complete
  // during the detach phase of this swap. Suppress its completion until swap is
  // over, at which point its completion will be correctly dependent on its
  // newly swapped-in child.
  auto* parent_local_frame = DynamicTo<LocalFrame>(parent_.Get());
  std::unique_ptr<IncrementLoadEventDelayCount> delay_parent_load =
      parent_local_frame ? std::make_unique<IncrementLoadEventDelayCount>(
                               *parent_local_frame->GetDocument())
                         : nullptr;

  // Unload the current Document in this frame: this calls unload handlers,
  // detaches child frames, etc. Since this runs script, make sure this frame
  // wasn't detached before continuing with the swap.
  if (!Detach(FrameDetachType::kSwap)) {
    // If the Swap() fails, it should be because the frame has been detached
    // already. Otherwise the caller will not detach the frame when we return
    // false, and the browser and renderer will disagree about the destruction
    // of |this|.
    CHECK(IsDetached());
    return false;
  }

  // Otherwise, on a successful `Detach()` for swap, `this` is now detached--but
  // crucially--still linked into the frame tree.

  if (provisional_frame_) {
    // `this` is about to be replaced, so if `provisional_frame_` is set, it
    // should match `frame` which is being swapped in.
    DCHECK_EQ(provisional_frame_, WebFrame::ToCoreFrame(*new_web_frame));
    provisional_frame_ = nullptr;
  }

  v8::HandleScope handle_scope(page->GetAgentGroupScheduler().Isolate());
  WindowProxyManager::GlobalProxyVector global_proxies;
  GetWindowProxyManager()->ReleaseGlobalProxies(global_proxies);

  if (new_web_frame->IsWebRemoteFrame()) {
    DCHECK(remote_frame_host && remote_frame_receiver);
    CHECK(!WebFrame::ToCoreFrame(*new_web_frame));
    To<WebRemoteFrameImpl>(new_web_frame)
        ->InitializeCoreFrame(*page, owner, WebFrame::FromCoreFrame(parent_),
                              nullptr, FrameInsertType::kInsertLater, name,
                              &window_agent_factory(), devtools_frame_token_,
                              std::move(remote_frame_host),
                              std::move(remote_frame_receiver));
    // At this point, a `RemoteFrame` will have already updated
    // `Page::MainFrame()` or `FrameOwner::ContentFrame()` as appropriate, and
    // its `parent_` pointer is also populated.
  } else {
    // This is local frame created by `WebLocalFrame::CreateProvisional()`. The
    // `parent` pointer was set when it was constructed; however,
    // `Page::MainFrame()` or `FrameOwner::ContentFrame()` updates are deferred
    // until after `new_frame` is linked into the frame tree.
    // TODO(dcheng): Make local and remote frame updates more uniform.
    DCHECK(!remote_frame_host && !remote_frame_receiver);
  }

  Frame* new_frame = WebFrame::ToCoreFrame(*new_web_frame);
  CHECK(new_frame);

  // At this point, `new_frame->parent_` is correctly set, but `new_frame`'s
  // sibling pointers are both still null and not yet updated. In addition, the
  // parent frame (if any) still has not updated its `first_child_` and
  // `last_child_` pointers.
  CHECK_EQ(new_frame->parent_, parent_);
  CHECK(!new_frame->previous_sibling_);
  CHECK(!new_frame->next_sibling_);
  if (previous_sibling_) {
    previous_sibling_->next_sibling_ = new_frame;
  }
  swap(previous_sibling_, new_frame->previous_sibling_);
  if (next_sibling_) {
    next_sibling_->previous_sibling_ = new_frame;
  }
  swap(next_sibling_, new_frame->next_sibling_);

  if (parent_) {
    if (parent_->first_child_ == this) {
      parent_->first_child_ = new_frame;
    }
    if (parent_->last_child_ == this) {
      parent_->last_child_ = new_frame;
    }
    // Not strictly necessary, but keep state as self-consistent as possible.
    parent_ = nullptr;
  }

  if (Frame* opener = opener_) {
    SetOpenerDoNotNotify(nullptr);
    new_frame->SetOpenerDoNotNotify(opener);
  }
  opened_frame_tracker_.TransferTo(new_frame);

  // Clone the state of the current Frame into the one being swapped in.
  if (auto* new_local_frame = DynamicTo<LocalFrame>(new_frame)) {
    // A `LocalFrame` being swapped in is created provisionally, so
    // `Page::MainFrame()` or `FrameOwner::ContentFrame()` needs to be updated
    // to point to the newly swapped-in frame.
    DCHECK_EQ(owner, new_local_frame->Owner());
    if (owner) {
      owner->SetContentFrame(*new_local_frame);

      if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
        frame_owner_element->SetEmbeddedContentView(new_local_frame->View());
      }
    } else {
      Page* new_page = new_local_frame->GetPage();
      if (page != new_page) {
        // The new frame can only belong to a different Page when doing a main
        // frame LocalFrame <-> LocalFrame swap, where we want to detach the
        // LocalFrame of the old Page before swapping in the new provisional
        // LocalFrame into the new Page.
        CHECK(IsLocalFrame());

        // First, finish handling the old page. At this point, the old Page's
        // main LocalFrame had already been detached by the `Detach()` call
        // above, and we should create and swap in a placeholder RemoteFrame to
        // ensure the old Page still has a main frame until it gets deleted
        // later on, when its WebView gets deleted. Attach the newly created
        // placeholder RemoteFrame as the main frame of the old Page.
        WebRemoteFrame* old_page_placeholder_remote_frame =
            WebRemoteFrame::Create(mojom::blink::TreeScopeType::kDocument,
                                   RemoteFrameToken());
        To<WebRemoteFrameImpl>(old_page_placeholder_remote_frame)
            ->InitializeCoreFrame(
                *page, /*owner=*/nullptr, /*parent=*/nullptr,
                /*previous_sibling=*/nullptr, FrameInsertType::kInsertLater,
                name, &window_agent_factory(), devtools_frame_token_,
                mojo::NullAssociatedRemote(), mojo::NullAssociatedReceiver());
        page->SetMainFrame(
            WebFrame::ToCoreFrame(*old_page_placeholder_remote_frame));

        // On the new Page, we have a different placeholder main RemoteFrame,
        // which was created when the new Page's WebView was created from
        // AgentSchedulingGroup::CreateWebView(). The placeholder main
        // RemoteFrame needs to be detached before the new Page's provisional
        // LocalFrame can take its place as the new Page's main frame.
        CHECK_NE(new_page->MainFrame(), this);
        CHECK(new_page->MainFrame()->IsRemoteFrame());
        CHECK(!DynamicTo<RemoteFrame>(new_page->MainFrame())
                   ->IsRemoteFrameHostRemoteBound());
        // Trigger the detachment of the new page's placeholder main
        // RemoteFrame. Note that we also use `FrameDetachType::kSwap` here
        // instead of kRemove to avoid triggering destructive action on the new
        // Page and the provisional LocalFrame that will be swapped in (e.g.
        // clearing the opener, or detaching the provisional frame).
        new_page->MainFrame()->Detach(FrameDetachType::kSwap);
      }

      // Set the provisioanl LocalFrame to become the new page's main frame.
      new_page->SetMainFrame(new_local_frame);
      // This trace event is needed to detect the main frame of the
      // renderer in telemetry metrics. See crbug.com/692112#c11.
      TRACE_EVENT_INSTANT1("loading", "markAsMainFrame",
                           TRACE_EVENT_SCOPE_THREAD, "frame",
                           ::blink::ToTraceValue(new_local_frame));
    }
  }

  new_frame->GetWindowProxyManager()->SetGlobalProxies(global_proxies);

  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
    if (auto* new_local_frame = DynamicTo<LocalFrame>(new_frame)) {
      probe::FrameOwnerContentUpdated(new_local_frame, frame_owner_element);
    } else if (auto* old_local_frame = DynamicTo<LocalFrame>(this)) {
      // TODO(dcheng): What is this probe for? Shouldn't it happen *before*
      // detach?
      probe::FrameOwnerContentUpdated(old_local_frame, frame_owner_element);
    }
  }

  return true;
}

void Frame::RemoveChild(Frame* child) {
  CHECK_EQ(child->parent_, this);
  child->parent_ = nullptr;

  if (first_child_ == child) {
    first_child_ = child->next_sibling_;
  } else {
    CHECK(child->previous_sibling_)
        << " child " << child << " child->previous_sibling_ "
        << child->previous_sibling_;
    child->previous_sibling_->next_sibling_ = child->next_sibling_;
  }

  if (last_child_ == child) {
    last_child_ = child->previous_sibling_;
  } else {
    CHECK(child->next_sibling_);
    child->next_sibling_->previous_sibling_ = child->previous_sibling_;
  }

  child->previous_sibling_ = child->next_sibling_ = nullptr;

  Tree().InvalidateScopedChildCount();
  GetPage()->DecrementSubframeCount();
}

void Frame::DetachFromParent() {
  if (!Parent())
    return;

  // TODO(dcheng): This should really just check if there's a parent, and call
  // RemoveChild() if so. Once provisional frames are removed, this check can be
  // simplified to just check Parent(). See https://crbug.com/578349.
  if (auto* local_frame = DynamicTo<LocalFrame>(this)) {
    if (local_frame->IsProvisional()) {
      return;
    }
  }
  Parent()->RemoveChild(this);
}

}  // namespace blink
