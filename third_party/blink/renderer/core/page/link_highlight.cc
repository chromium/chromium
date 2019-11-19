// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/link_highlight.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"

namespace blink {

LinkHighlight::LinkHighlight(Page& owner) : page_(&owner) {}

LinkHighlight::~LinkHighlight() {
  RemoveHighlight();
}

void LinkHighlight::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

void LinkHighlight::RemoveHighlight() {
  if (impl_) {
    if (timeline_)
      timeline_->AnimationDestroyed(*impl_);
    if (auto* node = impl_->GetNode()) {
      if (auto* layout_object = node->GetLayoutObject())
        layout_object->SetNeedsPaintPropertyUpdate();
    }
  }
  impl_.reset();
}

void LinkHighlight::ResetForPageNavigation() {
  RemoveHighlight();
}

void LinkHighlight::SetTapHighlight(Node* node) {
  // Always clear any existing highlight when this is invoked, even if we
  // don't get a new target to highlight.
  RemoveHighlight();

  if (!node || !node->GetLayoutObject())
    return;

  Color highlight_color =
      node->GetLayoutObject()->StyleRef().TapHighlightColor();
  // Safari documentation for -webkit-tap-highlight-color says if the
  // specified color has 0 alpha, then tap highlighting is disabled.
  // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
  if (!highlight_color.Alpha())
    return;

  impl_ = std::make_unique<LinkHighlightImpl>(node);
  if (timeline_)
    timeline_->AnimationAttached(*impl_);
  node->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

LocalFrame* LinkHighlight::MainFrame() const {
  return GetPage().MainFrame() && GetPage().MainFrame()->IsLocalFrame()
             ? GetPage().DeprecatedLocalMainFrame()
             : nullptr;
}

void LinkHighlight::StartHighlightAnimationIfNeeded() {
  if (impl_)
    impl_->StartHighlightAnimationIfNeeded();

  if (auto* local_frame = MainFrame())
    GetPage().GetChromeClient().ScheduleAnimation(local_frame->View());
}

void LinkHighlight::AnimationHostInitialized(
    cc::AnimationHost& animation_host) {
  animation_host_ = &animation_host;
  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    timeline_ = std::make_unique<CompositorAnimationTimeline>();
    animation_host_->AddAnimationTimeline(timeline_->GetAnimationTimeline());
  }
}

void LinkHighlight::WillCloseAnimationHost() {
  RemoveHighlight();
  if (timeline_) {
    animation_host_->RemoveAnimationTimeline(timeline_->GetAnimationTimeline());
    timeline_.reset();
  }
  animation_host_ = nullptr;
}

bool LinkHighlight::NeedsHighlightEffectInternal(
    const LayoutObject& object) const {
  DCHECK(impl_);
  if (auto* node = impl_->GetNode())
    return node->GetLayoutObject() == &object;
  return false;
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

}  // namespace blink
