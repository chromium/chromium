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

#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

using namespace HTMLNames;

Frame::~Frame() {
  InstanceCounters::DecrementCounter(InstanceCounters::kFrameCounter);
  DCHECK(!owner_);
  DCHECK(IsDetached());
}

void Frame::Trace(blink::Visitor* visitor) {
  visitor->Trace(tree_node_);
  visitor->Trace(page_);
  visitor->Trace(owner_);
  visitor->Trace(window_proxy_manager_);
  visitor->Trace(dom_window_);
  visitor->Trace(client_);
}

void Frame::Detach(FrameDetachType type) {
  DCHECK(client_);
  // Detach() can be re-entered, so this can't simply DCHECK(IsAttached()).
  DCHECK(!IsDetached());
  lifecycle_.AdvanceTo(FrameLifecycle::kDetaching);

  DetachImpl(type);
  // Due to re-entrancy, |this| could have completed detaching already.
  // TODO(dcheng): This DCHECK is not always true. See https://crbug.com/838348.
  DCHECK(IsDetached() == !client_);
  if (!client_)
    return;

  detach_stack_ = base::debug::StackTrace();
  client_->SetOpener(nullptr);
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

HTMLFrameOwnerElement* Frame::DeprecatedLocalOwner() const {
  return owner_ && owner_->IsLocal() ? ToHTMLFrameOwnerElement(owner_)
                                     : nullptr;
}

static ChromeClient& GetEmptyChromeClient() {
  DEFINE_STATIC_LOCAL(Persistent<EmptyChromeClient>, client,
                      (EmptyChromeClient::Create()));
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

void Frame::NotifyUserActivationInLocalTree() {
  for (Frame* node = this; node; node = node->Tree().Parent())
    node->user_activation_state_.Activate();
}

bool Frame::ConsumeTransientUserActivationInLocalTree() {
  bool was_active = user_activation_state_.IsActive();

  // Note that consumption "touches" the whole frame tree, to guarantee that a
  // malicious subframe can't embed sub-subframes in a way that could allow
  // multiple consumptions per user activation.
  Frame& root = Tree().Top();
  for (Frame* node = &root; node; node = node->Tree().TraverseNext(&root))
    node->user_activation_state_.ConsumeIfActive();

  return was_active;
}

bool Frame::DeprecatedIsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  return GetSecurityContext()->IsFeatureEnabled(feature,
                                                ReportOptions::kDoNotReport);
}

bool Frame::DeprecatedIsFeatureEnabled(mojom::FeaturePolicyFeature feature,
                                       ReportOptions report_on_failure) const {
  return GetSecurityContext()->IsFeatureEnabled(feature, report_on_failure);
}

void Frame::SetOwner(FrameOwner* owner) {
  owner_ = owner;
  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
}

void Frame::UpdateInertIfPossible() {
  if (owner_ && owner_->IsLocal()) {
    ToHTMLFrameOwnerElement(owner_)->UpdateDistributionForFlatTreeTraversal();
    if (ToHTMLFrameOwnerElement(owner_)->IsInert())
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

const CString& Frame::ToTraceValue() {
  // token's ToString() is latin1.
  if (!trace_value_)
    trace_value_ = CString(devtools_frame_token_.ToString().c_str());
  return trace_value_.value();
}

Frame::Frame(FrameClient* client,
             Page& page,
             FrameOwner* owner,
             WindowProxyManager* window_proxy_manager)
    : tree_node_(this),
      page_(&page),
      owner_(owner),
      client_(client),
      window_proxy_manager_(window_proxy_manager),
      is_loading_(false),
      devtools_frame_token_(client->GetDevToolsFrameToken()),
      create_stack_(base::debug::StackTrace()) {
  InstanceCounters::IncrementCounter(InstanceCounters::kFrameCounter);

  if (owner_)
    owner_->SetContentFrame(*this);
  else
    page_->SetMainFrame(this);
}

STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebLocalFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap,
                   WebLocalFrameClient::DetachType::kSwap);
STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebRemoteFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap,
                   WebRemoteFrameClient::DetachType::kSwap);

}  // namespace blink
