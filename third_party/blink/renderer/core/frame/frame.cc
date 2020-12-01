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

#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// static
Frame* Frame::ResolveFrame(const base::UnguessableToken& frame_token) {
  if (!frame_token)
    return nullptr;

  // The frame token could refer to either a RemoteFrame or a LocalFrame, so
  // need to check both.
  auto* remote = RemoteFrame::FromFrameToken(frame_token);
  if (remote)
    return remote;

  auto* local = LocalFrame::FromFrameToken(frame_token);
  if (local)
    return local;

  return nullptr;
}

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
  visitor->Trace(navigation_rate_limiter_);
  visitor->Trace(window_agent_factory_);
  visitor->Trace(opened_frame_tracker_);
}

void Frame::Detach(FrameDetachType type) {
  DCHECK(client_);
  // Detach() can be re-entered, so this can't simply DCHECK(IsAttached()).
  DCHECK(!IsDetached());
  lifecycle_.AdvanceTo(FrameLifecycle::kDetaching);
  PageDismissalScope in_page_dismissal;

  DetachImpl(type);

  if (GetPage())
    GetPage()->GetFocusController().FrameDetached(this);

  // Due to re-entrancy, |this| could have completed detaching already.
  // TODO(dcheng): This DCHECK is not always true. See https://crbug.com/838348.
  DCHECK(IsDetached() == !client_);
  if (!client_)
    return;

  SetOpener(nullptr);
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
  embedding_token_ = base::nullopt;
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

bool Frame::IsCrossOriginToMainFrame() const {
  DCHECK(GetSecurityContext());
  const SecurityOrigin* security_origin =
      GetSecurityContext()->GetSecurityOrigin();
  return !security_origin->CanAccess(
      Tree().Top().GetSecurityContext()->GetSecurityOrigin());
}

bool Frame::IsCrossOriginToParentFrame() const {
  DCHECK(GetSecurityContext());
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
  if (Page* page = this->GetPage())
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
  }

  // See the "Same-origin Visibility" section in |UserActivationState| class
  // doc.
  auto* local_frame = DynamicTo<LocalFrame>(this);
  if (local_frame &&
      RuntimeEnabledFeatures::UserActivationSameOriginVisibilityEnabled()) {
    const SecurityOrigin* security_origin =
        local_frame->GetSecurityContext()->GetSecurityOrigin();

    Frame& root = Tree().Top();
    for (Frame* node = &root; node; node = node->Tree().TraverseNext(&root)) {
      auto* local_frame_node = DynamicTo<LocalFrame>(node);
      if (local_frame_node &&
          security_origin->CanAccess(
              local_frame_node->GetSecurityContext()->GetSecurityOrigin())) {
        node->user_activation_state_.Activate(notification_type);
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

  for (Frame* node = &root; node; node = node->Tree().TraverseNext(&root))
    node->user_activation_state_.ConsumeIfActive();

  return was_active;
}

void Frame::ClearUserActivationInFrameTree() {
  for (Frame* node = this; node; node = node->Tree().TraverseNext(this))
    node->user_activation_state_.Clear();
}

void Frame::SetOwner(FrameOwner* owner) {
  owner_ = owner;
  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
}

bool Frame::IsAdSubframe() const {
  return ad_frame_type_ != mojom::blink::AdFrameType::kNonAd;
}

bool Frame::IsAdRoot() const {
  return ad_frame_type_ == mojom::blink::AdFrameType::kRootAd;
}

void Frame::UpdateInertIfPossible() {
  if (auto* frame_owner_element =
          DynamicTo<HTMLFrameOwnerElement>(owner_.Get())) {
    frame_owner_element->UpdateDistributionForFlatTreeTraversal();
    if (frame_owner_element->IsInert())
      SetIsInert(true);
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
             const base::UnguessableToken& frame_token,
             WindowProxyManager* window_proxy_manager,
             WindowAgentFactory* inheriting_agent_factory)
    : tree_node_(this),
      page_(&page),
      owner_(owner),
      ad_frame_type_(mojom::blink::AdFrameType::kNonAd),
      client_(client),
      window_proxy_manager_(window_proxy_manager),
      parent_(parent),
      navigation_rate_limiter_(*this),
      window_agent_factory_(inheriting_agent_factory
                                ? inheriting_agent_factory
                                : MakeGarbageCollected<WindowAgentFactory>()),
      is_loading_(false),
      devtools_frame_token_(client->GetDevToolsFrameToken()),
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
  owner->SetRequiredCsp(properties->required_csp);
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

void Frame::ScheduleFormSubmission(FrameScheduler* scheduler,
                                   FormSubmission* form_submission) {
  form_submit_navigation_task_ = PostCancellableTask(
      *scheduler->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      WTF::Bind(&FormSubmission::Navigate, WrapPersistent(form_submission)));
}

void Frame::CancelFormSubmission() {
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

Frame* Frame::Top() {
  Frame* parent;
  for (parent = this; parent->Parent(); parent = parent->Parent()) {
  }
  return parent;
}

bool Frame::Swap(WebFrame* frame) {
  using std::swap;
  Frame* old_frame = this;
  if (!old_frame->IsAttached())
    return false;
  FrameOwner* owner = old_frame->Owner();
  FrameSwapScope frame_swap_scope(owner);
  Frame* new_frame_parent = WebFrame::ToCoreFrame(*frame) && frame->Parent()
                                ? WebFrame::ToCoreFrame(*frame->Parent())
                                : nullptr;

  Page* page = old_frame->GetPage();
  AtomicString name = old_frame->Tree().GetName();
  Frame* old_frame_opener = old_frame->Opener();
  Frame* old_frame_parent = old_frame->parent_;
  Frame* old_frame_previous_sibling = old_frame->previous_sibling_;
  Frame* old_frame_next_sibling = old_frame->next_sibling_;

  // Unload the current Document in this frame: this calls unload handlers,
  // detaches child frames, etc. Since this runs script, make sure this frame
  // wasn't detached before continuing with the swap.
  // FIXME: There is no unit test for this condition, so one needs to be
  // written.
  if (!old_frame->DetachDocument()) {
    // If the Swap() fails, it should be because the frame has been detached
    // already. Otherwise the caller will not detach the frame when we return
    // false, and the browser and renderer will disagree about the destruction
    // of |old_frame|.
    CHECK(!old_frame->IsAttached());
    return false;
  }

  // If there is a local parent, it might incorrectly declare itself complete
  // during the detach phase of this swap. Suppress its completion until swap is
  // over, at which point its completion will be correctly dependent on its
  // newly swapped-in child.
  auto* parent_local_frame = DynamicTo<LocalFrame>(parent_.Get());
  std::unique_ptr<IncrementLoadEventDelayCount> delay_parent_load =
      parent_local_frame ? std::make_unique<IncrementLoadEventDelayCount>(
                               *parent_local_frame->GetDocument())
                         : nullptr;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WindowProxyManager::GlobalProxyVector global_proxies;
  old_frame->GetWindowProxyManager()->ClearForSwap();
  old_frame->GetWindowProxyManager()->ReleaseGlobalProxies(global_proxies);

  // This must be before Detach so DidChangeOpener is not called.
  if (old_frame_opener)
    old_frame->SetOpenerDoNotNotify(nullptr);

  // Although the Document in this frame is now unloaded, many resources
  // associated with the frame itself have not yet been freed yet.
  old_frame->Detach(FrameDetachType::kSwap);
  if (frame->IsWebRemoteFrame()) {
    CHECK(!WebFrame::ToCoreFrame(*frame));
    To<WebRemoteFrameImpl>(frame)->InitializeCoreFrame(
        *page, owner, WebFrame::FromFrame(old_frame_parent), nullptr,
        FrameInsertType::kInsertLater, name,
        &old_frame->window_agent_factory());
  }

  Frame* new_frame = WebFrame::ToCoreFrame(*frame);
  CHECK(new_frame);

  // Swaps the |new_frame| and |old_frame| in their frame trees.
  // For the |old_frame|, we use the frame tree position prior to the Detach()
  // call.
  Frame* new_frame_previous_sibling = new_frame->previous_sibling_;
  Frame* new_frame_next_sibling = new_frame->next_sibling_;

  new_frame->parent_ = old_frame_parent;
  new_frame->previous_sibling_ = old_frame_previous_sibling;
  new_frame->next_sibling_ = old_frame_next_sibling;
  if (new_frame_previous_sibling) {
    new_frame_previous_sibling->next_sibling_ = old_frame;
  }
  if (new_frame_next_sibling) {
    new_frame_next_sibling->previous_sibling_ = old_frame;
  }
  if (new_frame_parent) {
    if (new_frame_parent->first_child_ == new_frame) {
      new_frame_parent->first_child_ = old_frame;
    }
    if (new_frame_parent->last_child_ == new_frame) {
      new_frame_parent->last_child_ = old_frame;
    }
  }
  old_frame->parent_ = new_frame_parent;
  old_frame->previous_sibling_ = new_frame_previous_sibling;
  old_frame->next_sibling_ = new_frame_next_sibling;
  if (old_frame_previous_sibling) {
    old_frame_previous_sibling->next_sibling_ = new_frame;
  }
  if (old_frame_next_sibling) {
    old_frame_next_sibling->previous_sibling_ = new_frame;
  }
  if (old_frame_parent) {
    if (old_frame_parent->first_child_ == old_frame) {
      old_frame_parent->first_child_ = new_frame;
    }
    if (old_frame_parent->last_child_ == old_frame) {
      old_frame_parent->last_child_ = new_frame;
    }
  }

  if (old_frame_opener) {
    new_frame->SetOpenerDoNotNotify(old_frame_opener);
  }
  opened_frame_tracker_.TransferTo(new_frame);

  // Clone the state of the current Frame into the one being swapped in.
  // FIXME: This is a bit clunky; this results in pointless decrements and
  // increments of connected subframes.
  if (auto* new_local_frame = DynamicTo<LocalFrame>(new_frame)) {
    // TODO(dcheng): in an ideal world, both branches would just use
    // WebFrame's initializeCoreFrame() helper. However, Blink
    // currently requires a 'provisional' local frame to serve as a
    // placeholder for loading state when swapping to a local frame.
    // In this case, the core LocalFrame is already initialized, so just
    // update a bit of state.
    DCHECK_EQ(owner, new_local_frame->Owner());
    if (owner) {
      owner->SetContentFrame(*new_local_frame);

      if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
        frame_owner_element->SetEmbeddedContentView(new_local_frame->View());
      }
    } else {
      Page* other_page = new_local_frame->GetPage();
      other_page->SetMainFrame(new_local_frame);
      // This trace event is needed to detect the main frame of the
      // renderer in telemetry metrics. See crbug.com/692112#c11.
      TRACE_EVENT_INSTANT1("loading", "markAsMainFrame",
                           TRACE_EVENT_SCOPE_THREAD, "frame",
                           ::blink::ToTraceValue(new_local_frame));
    }
  }

  new_frame->GetWindowProxyManager()->SetGlobalProxies(global_proxies);

  parent_ = nullptr;

  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
    if (auto* new_local_frame = DynamicTo<LocalFrame>(new_frame)) {
      probe::FrameOwnerContentUpdated(new_local_frame, frame_owner_element);
    } else if (auto* old_local_frame = DynamicTo<LocalFrame>(old_frame)) {
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

STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebRemoteFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap,
                   WebRemoteFrameClient::DetachType::kSwap);

}  // namespace blink
