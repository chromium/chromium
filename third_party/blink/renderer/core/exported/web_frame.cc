// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_frame.h"

#include <algorithm>
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/opened_frame_tracker.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

bool WebFrame::Swap(WebFrame* frame) {
  using std::swap;
  Frame* old_frame = ToCoreFrame(*this);
  if (!old_frame->IsAttached())
    return false;
  FrameOwner* owner = old_frame->Owner();
  FrameSwapScope frame_swap_scope(owner);

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
  auto* parent_web_local_frame = DynamicTo<WebLocalFrameImpl>(parent_);
  std::unique_ptr<IncrementLoadEventDelayCount> delay_parent_load =
      parent_web_local_frame
          ? std::make_unique<IncrementLoadEventDelayCount>(
                *parent_web_local_frame->GetFrame()->GetDocument())
          : nullptr;

  if (parent_) {
    if (parent_->first_child_ == this)
      parent_->first_child_ = frame;
    if (parent_->last_child_ == this)
      parent_->last_child_ = frame;
    // FIXME: This is due to the fact that the |frame| may be a provisional
    // local frame, because we don't know if the navigation will result in
    // an actual page or something else, like a download. The PlzNavigate
    // project will remove the need for provisional local frames.
    frame->parent_ = parent_;
  }

  if (previous_sibling_) {
    previous_sibling_->next_sibling_ = frame;
    swap(previous_sibling_, frame->previous_sibling_);
  }
  if (next_sibling_) {
    next_sibling_->previous_sibling_ = frame;
    swap(next_sibling_, frame->next_sibling_);
  }

  if (opener_) {
    frame->SetOpener(opener_);
    SetOpener(nullptr);
  }
  opened_frame_tracker_->TransferTo(frame);

  Page* page = old_frame->GetPage();
  AtomicString name = old_frame->Tree().GetName();

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WindowProxyManager::GlobalProxyVector global_proxies;
  old_frame->GetWindowProxyManager()->ClearForSwap();
  old_frame->GetWindowProxyManager()->ReleaseGlobalProxies(global_proxies);

  // Although the Document in this frame is now unloaded, many resources
  // associated with the frame itself have not yet been freed yet.
  old_frame->Detach(FrameDetachType::kSwap);

  // Clone the state of the current Frame into the one being swapped in.
  // FIXME: This is a bit clunky; this results in pointless decrements and
  // increments of connected subframes.
  if (auto* web_local_frame = DynamicTo<WebLocalFrameImpl>(frame)) {
    // TODO(dcheng): in an ideal world, both branches would just use
    // WebFrame's initializeCoreFrame() helper. However, Blink
    // currently requires a 'provisional' local frame to serve as a
    // placeholder for loading state when swapping to a local frame.
    // In this case, the core LocalFrame is already initialized, so just
    // update a bit of state.
    LocalFrame& local_frame = *web_local_frame->GetFrame();
    DCHECK_EQ(owner, local_frame.Owner());
    if (owner) {
      owner->SetContentFrame(local_frame);

      if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
        frame_owner_element->SetEmbeddedContentView(local_frame.View());
      }
    } else {
      Page* other_page = local_frame.GetPage();
      other_page->SetMainFrame(&local_frame);
      // This trace event is needed to detect the main frame of the
      // renderer in telemetry metrics. See crbug.com/692112#c11.
      TRACE_EVENT_INSTANT1("loading", "markAsMainFrame",
                           TRACE_EVENT_SCOPE_THREAD, "frame",
                           ToTraceValue(&local_frame));
    }
  } else {
    ToWebRemoteFrameImpl(frame)->InitializeCoreFrame(
        *page, owner, name, &old_frame->window_agent_factory());
  }

  Frame* new_frame = ToCoreFrame(*frame);

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

void WebFrame::Detach() {
  ToCoreFrame(*this)->Detach(FrameDetachType::kRemove);
}

WebSecurityOrigin WebFrame::GetSecurityOrigin() const {
  return WebSecurityOrigin(
      ToCoreFrame(*this)->GetSecurityContext()->GetSecurityOrigin());
}

void WebFrame::SetFrameOwnerPolicy(const FramePolicy& frame_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  To<RemoteFrameOwner>(ToCoreFrame(*this)->Owner())
      ->SetFramePolicy(frame_policy);
}

WebInsecureRequestPolicy WebFrame::GetInsecureRequestPolicy() const {
  return ToCoreFrame(*this)->GetSecurityContext()->GetInsecureRequestPolicy();
}

WebVector<unsigned> WebFrame::GetInsecureRequestToUpgrade() const {
  const SecurityContext::InsecureNavigationsSet& set =
      ToCoreFrame(*this)->GetSecurityContext()->InsecureNavigationsToUpgrade();
  return SecurityContext::SerializeInsecureNavigationSet(set);
}

void WebFrame::SetFrameOwnerProperties(
    const WebFrameOwnerProperties& properties) {
  // At the moment, this is only used to replicate frame owner properties
  // for frames with a remote owner.
  auto* owner = To<RemoteFrameOwner>(ToCoreFrame(*this)->Owner());

  Frame* frame = ToCoreFrame(*this);
  DCHECK(frame);

  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    local_frame->GetDocument()->WillChangeFrameOwnerProperties(
        properties.margin_width, properties.margin_height,
        static_cast<ScrollbarMode>(properties.scrolling_mode),
        properties.is_display_none);
  }

  owner->SetBrowsingContextContainerName(properties.name);
  owner->SetScrollingMode(properties.scrolling_mode);
  owner->SetMarginWidth(properties.margin_width);
  owner->SetMarginHeight(properties.margin_height);
  owner->SetAllowFullscreen(properties.allow_fullscreen);
  owner->SetAllowPaymentRequest(properties.allow_payment_request);
  owner->SetIsDisplayNone(properties.is_display_none);
  owner->SetRequiredCsp(properties.required_csp);
}

void WebFrame::Collapse(bool collapsed) {
  FrameOwner* owner = ToCoreFrame(*this)->Owner();
  To<HTMLFrameOwnerElement>(owner)->SetCollapsed(collapsed);
}

WebFrame* WebFrame::Opener() const {
  return opener_;
}

void WebFrame::SetOpener(WebFrame* opener) {
  if (opener_)
    opener_->opened_frame_tracker_->Remove(this);
  if (opener)
    opener->opened_frame_tracker_->Add(this);
  opener_ = opener;
}

void WebFrame::ClearOpener() {
  SetOpener(nullptr);
}

void WebFrame::InsertAfter(WebFrame* new_child, WebFrame* previous_sibling) {
  new_child->parent_ = this;

  WebFrame* next;
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

  ToCoreFrame(*this)->Tree().InvalidateScopedChildCount();
  ToCoreFrame(*this)->GetPage()->IncrementSubframeCount();
}

void WebFrame::AppendChild(WebFrame* child) {
  // TODO(dcheng): Original code asserts that the frames have the same Page.
  // We should add an equivalent check... figure out what.
  InsertAfter(child, last_child_);
}

void WebFrame::RemoveChild(WebFrame* child) {
  child->parent_ = nullptr;

  if (first_child_ == child)
    first_child_ = child->next_sibling_;
  else
    child->previous_sibling_->next_sibling_ = child->next_sibling_;

  if (last_child_ == child)
    last_child_ = child->previous_sibling_;
  else
    child->next_sibling_->previous_sibling_ = child->previous_sibling_;

  child->previous_sibling_ = child->next_sibling_ = nullptr;

  ToCoreFrame(*this)->Tree().InvalidateScopedChildCount();
  ToCoreFrame(*this)->GetPage()->DecrementSubframeCount();
}

void WebFrame::SetParent(WebFrame* parent) {
  parent_ = parent;
}

WebFrame* WebFrame::Parent() const {
  return parent_;
}

WebFrame* WebFrame::Top() const {
  WebFrame* frame = const_cast<WebFrame*>(this);
  for (WebFrame* parent = frame; parent; parent = parent->parent_)
    frame = parent;
  return frame;
}

WebFrame* WebFrame::FirstChild() const {
  return first_child_;
}

WebFrame* WebFrame::NextSibling() const {
  return next_sibling_;
}

WebFrame* WebFrame::TraverseNext() const {
  if (Frame* frame = ToCoreFrame(*this))
    return FromFrame(frame->Tree().TraverseNext());
  return nullptr;
}

WebFrame* WebFrame::FromFrameOwnerElement(const WebNode& web_node) {
  Node* node = web_node;

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node))
    return FromFrame(frame_owner->ContentFrame());
  return nullptr;
}

bool WebFrame::IsLoading() const {
  if (Frame* frame = ToCoreFrame(*this))
    return frame->IsLoading();
  return false;
}

WebFrame* WebFrame::FromFrame(Frame* frame) {
  if (!frame)
    return nullptr;

  if (auto* local_frame = DynamicTo<LocalFrame>(frame))
    return WebLocalFrameImpl::FromFrame(*local_frame);
  return WebRemoteFrameImpl::FromFrame(To<RemoteFrame>(*frame));
}

WebFrame::WebFrame(WebTreeScopeType scope)
    : scope_(scope),
      parent_(nullptr),
      previous_sibling_(nullptr),
      next_sibling_(nullptr),
      first_child_(nullptr),
      last_child_(nullptr),
      opener_(nullptr),
      opened_frame_tracker_(new OpenedFrameTracker) {}

WebFrame::~WebFrame() {
  opened_frame_tracker_.reset(nullptr);
}

void WebFrame::TraceFrame(Visitor* visitor, WebFrame* frame) {
  if (!frame)
    return;

  if (auto* web_local_frame = DynamicTo<WebLocalFrameImpl>(frame))
    visitor->Trace(web_local_frame);
  else
    visitor->Trace(ToWebRemoteFrameImpl(frame));
}

void WebFrame::TraceFrames(Visitor* visitor, WebFrame* frame) {
  DCHECK(frame);
  TraceFrame(visitor, frame->parent_);
  for (WebFrame* child = frame->FirstChild(); child;
       child = child->NextSibling())
    TraceFrame(visitor, child);
}

void WebFrame::Close() {
  opened_frame_tracker_->Dispose();
}

void WebFrame::DetachFromParent() {
  if (!Parent())
    return;

  // TODO(dcheng): This should really just check if there's a parent, and call
  // RemoveChild() if so. Once provisional frames are removed, this check can be
  // simplified to just check Parent(). See https://crbug.com/578349.
  if (IsWebLocalFrame() && ToWebLocalFrame()->IsProvisional())
    return;

  Parent()->RemoveChild(this);
}

Frame* WebFrame::ToCoreFrame(const WebFrame& frame) {
  if (auto* web_local_frame = DynamicTo<WebLocalFrameImpl>(&frame))
    return web_local_frame->GetFrame();
  if (frame.IsWebRemoteFrame())
    return ToWebRemoteFrameImpl(frame).GetFrame();
  NOTREACHED();
  return nullptr;
}

STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::kAuto,
                   ScrollbarMode::kAuto);
STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::kAlwaysOff,
                   ScrollbarMode::kAlwaysOff);
STATIC_ASSERT_ENUM(WebFrameOwnerProperties::ScrollingMode::kAlwaysOn,
                   ScrollbarMode::kAlwaysOn);

}  // namespace blink
