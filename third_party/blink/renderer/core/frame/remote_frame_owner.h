// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_OWNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_OWNER_H_

#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Helper class to bridge communication for a frame with a remote parent.
// Currently, it serves two purposes:
// 1. Allows the local frame's loader to retrieve sandbox flags associated with
//    its owner element in another process.
// 2. Trigger a load event on its owner element once it finishes a load.
class CORE_EXPORT RemoteFrameOwner final
    : public GarbageCollected<RemoteFrameOwner>,
      public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(RemoteFrameOwner);

 public:
  RemoteFrameOwner(const FramePolicy&,
                   const WebFrameOwnerProperties&,
                   FrameOwnerElementType frame_owner_element_type);

  // FrameOwner overrides:
  Frame* ContentFrame() const override { return frame_.Get(); }
  void SetContentFrame(Frame&) override;
  void ClearContentFrame() override;
  const FramePolicy& GetFramePolicy() const override { return frame_policy_; }
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void DispatchLoad() override;
  bool CanRenderFallbackContent() const override {
    return frame_owner_element_type_ == FrameOwnerElementType::kObject;
  }
  void RenderFallbackContent(Frame*) override;
  void IntrinsicSizingInfoChanged() override;
  void SetNeedsOcclusionTracking(bool) override;

  AtomicString BrowsingContextContainerName() const override {
    return browsing_context_container_name_;
  }
  ScrollbarMode ScrollingMode() const override { return scrolling_; }
  int MarginWidth() const override { return margin_width_; }
  int MarginHeight() const override { return margin_height_; }
  bool AllowFullscreen() const override { return allow_fullscreen_; }
  bool AllowPaymentRequest() const override { return allow_payment_request_; }
  bool IsDisplayNone() const override { return is_display_none_; }
  AtomicString RequiredCsp() const override { return required_csp_; }
  bool ShouldLazyLoadChildren() const final;

  void SetFramePolicy(const FramePolicy& frame_policy) {
    frame_policy_ = frame_policy;
  }
  void SetBrowsingContextContainerName(const WebString& name) {
    browsing_context_container_name_ = name;
  }
  void SetScrollingMode(WebFrameOwnerProperties::ScrollingMode);
  void SetMarginWidth(int margin_width) { margin_width_ = margin_width; }
  void SetMarginHeight(int margin_height) { margin_height_ = margin_height; }
  void SetAllowFullscreen(bool allow_fullscreen) {
    allow_fullscreen_ = allow_fullscreen;
  }
  void SetAllowPaymentRequest(bool allow_payment_request) {
    allow_payment_request_ = allow_payment_request;
  }
  void SetIsDisplayNone(bool is_display_none) {
    is_display_none_ = is_display_none;
  }
  void SetRequiredCsp(const WebString& required_csp) {
    required_csp_ = required_csp;
  }

  void Trace(blink::Visitor*) override;

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already HTMLFrameOwnerElement.
  bool IsLocal() const override { return false; }
  bool IsRemote() const override { return true; }

  Member<Frame> frame_;
  FramePolicy frame_policy_;
  AtomicString browsing_context_container_name_;
  ScrollbarMode scrolling_;
  int margin_width_;
  int margin_height_;
  bool allow_fullscreen_;
  bool allow_payment_request_;
  bool is_display_none_;
  bool needs_occlusion_tracking_;
  WebString required_csp_;
  const FrameOwnerElementType frame_owner_element_type_;
};

template <>
struct DowncastTraits<RemoteFrameOwner> {
  static bool AllowFrom(const FrameOwner& owner) { return owner.IsRemote(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_OWNER_H_
