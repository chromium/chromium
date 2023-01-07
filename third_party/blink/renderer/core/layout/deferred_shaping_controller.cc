// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/deferred_shaping_controller.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/deferred_shaping.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

constexpr base::TimeDelta kMaximumDeferDuration = base::Seconds(5);

// static
DeferredShapingController* DeferredShapingController::From(
    const Document& document) {
  if (const auto* view = document.GetLayoutView())
    return &view->GetDeferredShapingController();
  return nullptr;
}

// static
DeferredShapingController& DeferredShapingController::From(
    const NGLayoutInputNode input_node) {
  return input_node.GetLayoutBox()->View()->GetDeferredShapingController();
}

DeferredShapingController::DeferredShapingController(Document& document)
    : document_(document) {}

void DeferredShapingController::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(deferred_elements_);
}

void DeferredShapingController::DisallowDeferredShaping() {
  DCHECK_EQ(CurrentViewportBottom(), kIndefiniteSize);
  DCHECK_EQ(CurrentMinimumTop(), LayoutUnit());
  default_allow_deferred_shaping_ = false;
}

void DeferredShapingController::RegisterDeferred(Element& element) {
  deferred_elements_.insert(&element);
}

bool DeferredShapingController::IsRegisteredDeferred(Element& element) const {
  return !deferred_elements_.empty() && deferred_elements_.Contains(&element);
}

void DeferredShapingController::UnregisterDeferred(Element& element) {
  deferred_elements_.erase(&element);
}

void DeferredShapingController::PerformPostLayoutTask() {
  if (deferred_elements_.size() <= 0)
    return;
  DCHECK(RuntimeEnabledFeatures::DeferredShapingEnabled());
  DEFERRED_SHAPING_VLOG(1) << "Deferred " << deferred_elements_.size()
                           << " elements";
  UseCounter::Count(*document_, WebFeature::kDeferredShapingWorked);
  reshaping_task_handle_ = PostDelayedCancellableTask(
      *document_->GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::BindOnce(&DeferredShapingController::ReshapeAllDeferred,
                    WrapWeakPersistent(this), ReshapeReason::kLastResort),
      kMaximumDeferDuration);
}

void DeferredShapingController::OnFirstContentfulPaint() {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  if (!document_->HasFinishedParsing())
    return;
  if (!default_allow_deferred_shaping_ && deferred_elements_.empty())
    return;
  default_allow_deferred_shaping_ = false;
  // Cancels the last resort task.
  reshaping_task_handle_.Cancel();
  reshaping_task_handle_ = PostCancellableTask(
      *document_->GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::BindOnce(&DeferredShapingController::ReshapeAllDeferred,
                    WrapWeakPersistent(this), ReshapeReason::kFcp));
}

size_t DeferredShapingController::ReshapeAllDeferredInternal() {
  if (deferred_elements_.empty())
    return 0;
  size_t count = 0;
  for (auto& element : deferred_elements_) {
    if (!element->isConnected())
      continue;
    LayoutBox* box = element->GetLayoutBox();
    if (!box || !box->IsShapingDeferred())
      continue;
    ++count;
    box->MarkContainerChainForLayout();
    box->SetIntrinsicLogicalWidthsDirty();
    box->SetChildNeedsLayout();
    // Make sure we don't use cached NGFragmentItem objects.
    box->DisassociatePhysicalFragments();
    box->ClearLayoutResults();
  }
  deferred_elements_.clear();
  return count;
}

void DeferredShapingController::ReshapeAllDeferred(ReshapeReason reason) {
  default_allow_deferred_shaping_ = false;
  size_t count = ReshapeAllDeferredInternal();
  if (count == 0)
    return;

  const char* reason_string;
  WebFeature feature = WebFeature::kMaxValue;
  switch (reason) {
    case ReshapeReason::kComputedStyle:
      reason_string = "computed style";
      feature = WebFeature::kDeferredShaping2ReshapedByComputedStyle;
      break;
    case ReshapeReason::kDomContentLoaded:
      reason_string = "DOMContentLoaded after FCP";
      feature = WebFeature::kDeferredShaping2ReshapedByDomContentLoaded;
      break;
    case ReshapeReason::kFcp:
      reason_string = "FCP after DOMContentLoaded";
      feature = WebFeature::kDeferredShaping2ReshapedByFcp;
      break;
    case ReshapeReason::kFragmentAnchor:
      reason_string = "fragment anchor";
      feature = WebFeature::kDeferredShaping2DisabledByFragmentAnchor;
      break;
    case ReshapeReason::kFocus:
      reason_string = "focus";
      feature = WebFeature::kDeferredShaping2ReshapedByFocus;
      break;
    case ReshapeReason::kGeometryApi:
      reason_string = "geometry APIs";
      feature = WebFeature::kDeferredShaping2ReshapedByGeometry;
      break;
    case ReshapeReason::kInspector:
      reason_string = "inspector";
      feature = WebFeature::kDeferredShaping2ReshapedByInspector;
      break;
    case ReshapeReason::kLastResort:
      reason_string = "the last resort";
      feature = WebFeature::kDeferredShaping2ReshapedByLastResort;
      break;
    case ReshapeReason::kPrinting:
      reason_string = "printing";
      feature = WebFeature::kDeferredShaping2ReshapedByPrinting;
      break;
    case ReshapeReason::kScrollingApi:
      reason_string = "scrolling APIs";
      feature = WebFeature::kDeferredShaping2ReshapedByScrolling;
      break;
    case ReshapeReason::kTesting:
      reason_string = "a test";
      break;
  }
  if (feature != WebFeature::kMaxValue) {
    UseCounter::Count(*document_, feature);
  }
  DEFERRED_SHAPING_VLOG(1) << "Reshaped all " << count << " elements by "
                           << reason_string;
  return;
}

void DeferredShapingController::ReshapeDeferred(ReshapeReason reason,
                                                const Node& target,
                                                CSSPropertyID property_id) {
  ReshapeAllDeferred(reason);
}

void DeferredShapingController::ReshapeDeferredForWidth(
    const LayoutObject& object) {
  ReshapeAllDeferred(ReshapeReason::kGeometryApi);
}

void DeferredShapingController::ReshapeDeferredForHeight(
    const LayoutObject& object) {
  ReshapeAllDeferred(ReshapeReason::kGeometryApi);
}

void DeferredShapingController::OnResizeFrame() {
  // This function does not clear default_allow_deferred_shaping_.
  // We don't need precise geometry of a specific element, and it's ok
  // to defer elements after the resize.
  size_t count = ReshapeAllDeferredInternal();
  if (count <= 0)
    return;
  DEFERRED_SHAPING_VLOG(1) << "Reshaped all " << count
                           << " elements by resizing";
}

void DeferredShapingController::OnFocus(const Element& element) {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  // If the element is in the viewport, we don't need to reshape deferred
  // elements.
  if (const LayoutBox* box = element.GetLayoutBox()) {
    if (box->EverHadLayout()) {
      gfx::RectF rect =
          element.GetBoundingClientRectNoLifecycleUpdateNoAdjustment();
      // We don't take into account of scroll offset intentionally. Scrolled
      // viewport might contain deferred elements.
      if (rect.y() < box->View()->ViewHeight())
        return;
    }
  }
  ReshapeAllDeferred(ReshapeReason::kFocus);
}

}  // namespace blink
