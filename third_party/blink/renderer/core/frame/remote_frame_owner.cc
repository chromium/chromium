// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"

#include "third_party/blink/public/platform/web_resource_timing_info.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

RemoteFrameOwner::RemoteFrameOwner(
    SandboxFlags flags,
    const ParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties,
    FrameOwnerElementType frame_owner_element_type)
    : sandbox_flags_(flags),
      browsing_context_container_name_(
          static_cast<String>(frame_owner_properties.name)),
      scrolling_(
          static_cast<ScrollbarMode>(frame_owner_properties.scrolling_mode)),
      margin_width_(frame_owner_properties.margin_width),
      margin_height_(frame_owner_properties.margin_height),
      allow_fullscreen_(frame_owner_properties.allow_fullscreen),
      allow_payment_request_(frame_owner_properties.allow_payment_request),
      is_display_none_(frame_owner_properties.is_display_none),
      required_csp_(frame_owner_properties.required_csp),
      container_policy_(container_policy),
      frame_owner_element_type_(frame_owner_element_type) {}

void RemoteFrameOwner::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  FrameOwner::Trace(visitor);
}

void RemoteFrameOwner::SetScrollingMode(
    WebFrameOwnerProperties::ScrollingMode mode) {
  scrolling_ = static_cast<ScrollbarMode>(mode);
}

void RemoteFrameOwner::SetContentFrame(Frame& frame) {
  frame_ = &frame;
}

void RemoteFrameOwner::ClearContentFrame() {
  DCHECK_EQ(frame_->Owner(), this);
  frame_ = nullptr;
}

void RemoteFrameOwner::AddResourceTiming(const ResourceTimingInfo& info) {
  LocalFrame* frame = ToLocalFrame(frame_);
  WebResourceTimingInfo resource_timing = Performance::GenerateResourceTiming(
      *frame->Tree().Parent()->GetSecurityContext()->GetSecurityOrigin(), info,
      *frame->GetDocument());
  frame->Client()->ForwardResourceTimingToParent(resource_timing);
}

void RemoteFrameOwner::DispatchLoad() {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(ToLocalFrame(*frame_));
  web_frame->Client()->DispatchLoad();
}

void RemoteFrameOwner::RenderFallbackContent(Frame* failed_frame) {
  if (frame_owner_element_type_ != FrameOwnerElementType::kObject)
    return;
  DCHECK(failed_frame->IsLocalFrame());
  LocalFrame* local_frame = ToLocalFrame(failed_frame);
  DCHECK(local_frame->IsProvisional() || ContentFrame() == local_frame);
  WebLocalFrameImpl::FromFrame(local_frame)
      ->Client()
      ->RenderFallbackContentInParentProcess();
}

void RemoteFrameOwner::IntrinsicSizingInfoChanged() {
  LocalFrame& local_frame = ToLocalFrame(*frame_);
  IntrinsicSizingInfo intrinsic_sizing_info;
  bool result =
      local_frame.View()->GetIntrinsicSizingInfo(intrinsic_sizing_info);
  // By virtue of having been invoked, GetIntrinsicSizingInfo() should always
  // succeed here.
  DCHECK(result);
  WebLocalFrameImpl::FromFrame(local_frame)
      ->FrameWidgetImpl()
      ->IntrinsicSizingInfoChanged(intrinsic_sizing_info);
}

bool RemoteFrameOwner::ShouldLazyLoadChildren() const {
  // Don't use lazy load for children inside an OOPIF, since there's a good
  // chance that the parent FrameOwner was previously deferred by lazy load
  // and then loaded in for whatever reason.
  return false;
}

}  // namespace blink
