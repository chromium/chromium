// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_frame.h"

#include <algorithm>
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
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
  return ToCoreFrame(*this)->Swap(frame);
}

void WebFrame::Detach() {
  ToCoreFrame(*this)->Detach(FrameDetachType::kRemove);
}

WebSecurityOrigin WebFrame::GetSecurityOrigin() const {
  return WebSecurityOrigin(
      ToCoreFrame(*this)->GetSecurityContext()->GetSecurityOrigin());
}

mojom::blink::InsecureRequestPolicy WebFrame::GetInsecureRequestPolicy() const {
  return ToCoreFrame(*this)->GetSecurityContext()->GetInsecureRequestPolicy();
}

WebVector<unsigned> WebFrame::GetInsecureRequestToUpgrade() const {
  const SecurityContext::InsecureNavigationsSet& set =
      ToCoreFrame(*this)->GetSecurityContext()->InsecureNavigationsToUpgrade();
  return SecurityContext::SerializeInsecureNavigationSet(set);
}

WebFrame* WebFrame::Opener() const {
  return FromFrame(ToCoreFrame(*this)->Opener());
}

void WebFrame::ClearOpener() {
  ToCoreFrame(*this)->SetOpenerDoNotNotify(nullptr);
}

WebFrame* WebFrame::Parent() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->Parent());
}

WebFrame* WebFrame::Top() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->Top());
}

WebFrame* WebFrame::FirstChild() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->FirstChild());
}

WebFrame* WebFrame::LastChild() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->LastChild());
}

WebFrame* WebFrame::NextSibling() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->NextSibling());
}

WebFrame* WebFrame::PreviousSibling() const {
  Frame* core_frame = ToCoreFrame(*this);
  CHECK(core_frame);
  return FromFrame(core_frame->PreviousSibling());
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

WebFrame::WebFrame(mojom::blink::TreeScopeType scope,
                   const base::UnguessableToken& frame_token)
    : scope_(scope), frame_token_(frame_token) {
  DCHECK(frame_token_);
}

void WebFrame::Close() {
}

Frame* WebFrame::ToCoreFrame(const WebFrame& frame) {
  if (auto* web_local_frame = DynamicTo<WebLocalFrameImpl>(&frame))
    return web_local_frame->GetFrame();
  if (frame.IsWebRemoteFrame())
    return To<WebRemoteFrameImpl>(frame).GetFrame();
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
