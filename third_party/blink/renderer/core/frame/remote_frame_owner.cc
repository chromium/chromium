// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

RemoteFrameOwner::RemoteFrameOwner(
    const FramePolicy& frame_policy,
    const WebFrameOwnerProperties& frame_owner_properties)
    : frame_policy_(frame_policy),
      browsing_context_container_name_(
          static_cast<String>(frame_owner_properties.name)),
      scrollbar_(frame_owner_properties.scrollbar_mode),
      margin_width_(frame_owner_properties.margin_width),
      margin_height_(frame_owner_properties.margin_height),
      allow_fullscreen_(frame_owner_properties.allow_fullscreen),
      allow_payment_request_(frame_owner_properties.allow_payment_request),
      is_display_none_(frame_owner_properties.is_display_none),
      color_scheme_(frame_owner_properties.color_scheme),
      preferred_color_scheme_(frame_owner_properties.preferred_color_scheme),
      needs_occlusion_tracking_(false) {}

void RemoteFrameOwner::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  FrameOwner::Trace(visitor);
}

void RemoteFrameOwner::SetScrollbarMode(mojom::blink::ScrollbarMode mode) {
  scrollbar_ = mode;
}

void RemoteFrameOwner::SetContentFrame(Frame& frame) {
  frame_ = &frame;
}

void RemoteFrameOwner::ClearContentFrame() {
  DCHECK_EQ(frame_->Owner(), this);
  frame_ = nullptr;
}

void RemoteFrameOwner::AddResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info) {
  DCHECK(info);
  LocalFrame* frame = To<LocalFrame>(frame_.Get());
  frame->GetLocalFrameHostRemote().ForwardResourceTimingToParent(
      std::move(info));
}

void RemoteFrameOwner::DispatchLoad() {
  auto& local_frame_host = To<LocalFrame>(*frame_).GetLocalFrameHostRemote();
  local_frame_host.DispatchLoad();
}

void RemoteFrameOwner::IntrinsicSizingInfoChanged() {
  LocalFrame& local_frame = To<LocalFrame>(*frame_);
  IntrinsicSizingInfo intrinsic_sizing_info;
  bool result =
      local_frame.View()->GetIntrinsicSizingInfo(intrinsic_sizing_info);
  // By virtue of having been invoked, GetIntrinsicSizingInfo() should always
  // succeed here.
  DCHECK(result);

  auto sizing_info = mojom::blink::IntrinsicSizingInfo::New(
      intrinsic_sizing_info.size, intrinsic_sizing_info.aspect_ratio,
      intrinsic_sizing_info.has_width, intrinsic_sizing_info.has_height);
  WebLocalFrameImpl::FromFrame(local_frame)
      ->FrameWidgetImpl()
      ->IntrinsicSizingInfoChanged(std::move(sizing_info));
}

void RemoteFrameOwner::SetNeedsOcclusionTracking(bool needs_tracking) {
  if (needs_tracking == needs_occlusion_tracking_)
    return;
  needs_occlusion_tracking_ = needs_tracking;
  LocalFrame* local_frame = To<LocalFrame>(frame_.Get());
  local_frame->GetLocalFrameHostRemote().SetNeedsOcclusionTracking(
      needs_tracking);
}

bool RemoteFrameOwner::ShouldLazyLoadChildren() const {
  // Don't use lazy load for children inside an OOPIF, since there's a good
  // chance that the parent FrameOwner was previously deferred by lazy load
  // and then loaded in for whatever reason.
  return false;
}

}  // namespace blink
