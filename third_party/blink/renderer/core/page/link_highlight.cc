// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/link_highlight.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"

namespace blink {

LinkHighlight::LinkHighlight(Page& owner) : page_(&owner) {}

LinkHighlight::~LinkHighlight() {
  RemoveHighlight();
}

void LinkHighlight::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

void LinkHighlight::RemoveHighlight() {
  if (!impl_)
    return;

  if (timeline_ && impl_->GetCompositorAnimation())
    timeline_->DetachAnimation(impl_->GetCompositorAnimation()->CcAnimation());

  impl_.reset();
}

void LinkHighlight::ResetForPageNavigation() {
  RemoveHighlight();
}

void LinkHighlight::SetTapHighlight(Node* node) {
  // Always clear any existing highlight when this is invoked, even if we
  // don't get a new target to highlight.
  RemoveHighlight();

  if (!node)
    return;

  DCHECK(node->GetLayoutObject());
  DCHECK(!node->IsTextNode());

  Color highlight_color =
      node->GetLayoutObject()->StyleRef().VisitedDependentColor(
          GetCSSPropertyWebkitTapHighlightColor());
  // Safari documentation for -webkit-tap-highlight-color says if the
  // specified color has 0 alpha, then tap highlighting is disabled.
  // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
  if (highlight_color.IsFullyTransparent()) {
    return;
  }

  impl_ = std::make_unique<LinkHighlightImpl>(node);
  if (timeline_ && impl_->GetCompositorAnimation())
    timeline_->AttachAnimation(impl_->GetCompositorAnimation()->CcAnimation());
}

LocalFrame* LinkHighlight::MainFrame() const {
  return GetPage().MainFrame() && GetPage().MainFrame()->IsLocalFrame()
             ? GetPage().DeprecatedLocalMainFrame()
             : nullptr;
}

void LinkHighlight::UpdateOpacityAndRequestAnimation() {
  if (impl_)
    impl_->UpdateOpacityAndRequestAnimation();

  if (auto* local_frame = MainFrame())
    GetPage().GetChromeClient().ScheduleAnimation(local_frame->View());
}

void LinkHighlight::AnimationHostInitialized(
    cc::AnimationHost& animation_host) {
  animation_host_ = &animation_host;
  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    timeline_ = cc::AnimationTimeline::Create(
        cc::AnimationIdProvider::NextTimelineId());
    animation_host_->AddAnimationTimeline(timeline_.get());
  }
}

void LinkHighlight::WillCloseAnimationHost() {
  RemoveHighlight();
  if (timeline_) {
    animation_host_->RemoveAnimationTimeline(timeline_.get());
    timeline_.reset();
  }
  animation_host_ = nullptr;
}

bool LinkHighlight::IsHighlightingInternal(const LayoutObject& object) const {
  DCHECK(impl_);
  return &object == impl_->GetLayoutObject();
}

void LinkHighlight::UpdateBeforePrePaint() {
  if (impl_)
    impl_->UpdateBeforePrePaint();
}

void LinkHighlight::UpdateAfterPrePaint() {
  if (impl_)
    impl_->UpdateAfterPrePaint();
}

void LinkHighlight::Paint(GraphicsContext& context) const {
  if (impl_)
    impl_->Paint(context);
}

void LinkHighlight::UpdateAfterPaint(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  if (impl_)
    impl_->UpdateAfterPaint(paint_artifact_compositor);
}

}  // namespace blink
