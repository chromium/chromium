// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

// Delay to ensure the canvas element has had a chance to update its
// accessibility related information.
constexpr base::TimeDelta kUMATimerDelay = base::Seconds(5);

}  // namespace

HTMLCanvasAccessibilityManager::HTMLCanvasAccessibilityManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_ignored,
    HTMLCanvasElement* canvas_element)
    : is_ignored_(is_ignored),
      uma_timer_(std::move(task_runner),
                 this,
                 &HTMLCanvasAccessibilityManager::RecordUma),
      canvas_element_(canvas_element) {
  UpdateHasFallbackElementContent();

  has_layoutsubtree_ = canvas_element->layoutSubtree();

  ReadAriaAttributes();

  SetVisible(canvas_element->IsDisplayed());
  is_initialized_ = true;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::UpdateHasFallbackElementContent() {
  bool has_fallback_element_content =
      ElementTraversal::FirstChild(*canvas_element_) != nullptr;
  if (has_fallback_element_content_ == has_fallback_element_content) {
    return;
  }
  has_fallback_element_content_ = has_fallback_element_content;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::ReadAriaAttributes() {
  if (canvas_element_->FastHasAttribute(html_names::kAriaHiddenAttr)) {
    if (canvas_element_->FastGetAttribute(html_names::kAriaHiddenAttr) ==
        "true") {
      is_aria_hidden_ = true;
    } else {
      has_aria_attributes_ = true;
    }
  }

  if (!has_aria_attributes_) {
    has_aria_attributes_ =
        canvas_element_->FastHasAttribute(html_names::kRoleAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaLabelAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaLabelledbyAttr) ||
        canvas_element_->FastHasAttribute(html_names::kAriaDescribedbyAttr) ||
        canvas_element_->FastHasAttribute(html_names::kTitleAttr);
  }

  OnUpdate();
}

void HTMLCanvasAccessibilityManager::SetVisible(bool is_visible) {
  if (is_visible_ == is_visible) {
    return;
  }
  is_visible_ = is_visible;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::SetHasLayoutSubtree(
    bool has_layoutsubtree) {
  if (has_layoutsubtree_ == has_layoutsubtree) {
    return;
  }
  has_layoutsubtree_ = has_layoutsubtree;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnResize() {
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnUpdate() {
  if (!is_initialized_) {
    return;
  }

  if (!is_visible_) {
    SetHeuristicResult(HeuristicResult::kIsNotVisible);
    return;
  }

  if (is_ignored_ || is_aria_hidden_) {
    SetHeuristicResult(HeuristicResult::kIsIgnoredOrAriaHidden);
    return;
  }

  // Minimum size canvas to assume it can have text.
  const float kMinPixelSizeForTextVisibility = 10;
  gfx::SizeF layout_size;
  if (auto* layout_box =
          DynamicTo<LayoutBox>(canvas_element_->GetLayoutObject())) {
    layout_size.set_width(layout_box->LogicalWidth().ToFloat());
    layout_size.set_height(layout_box->LogicalHeight().ToFloat());
  }
  if (layout_size.width() < kMinPixelSizeForTextVisibility ||
      layout_size.height() < kMinPixelSizeForTextVisibility) {
    SetHeuristicResult(HeuristicResult::kTooSmall);
    return;
  }

  if (has_layoutsubtree_) {
    SetHeuristicResult(HeuristicResult::kHasLayoutSubtree);
    return;
  }

  if (has_fallback_element_content_) {
    SetHeuristicResult(HeuristicResult::kHasFallbackContent);
    return;
  }

  if (has_aria_attributes_) {
    SetHeuristicResult(HeuristicResult::kHasAriaAttributes);
    return;
  }

  SetHeuristicResult(HeuristicResult::kNeedsA11ySupport);
}

void HTMLCanvasAccessibilityManager::SetHeuristicResult(
    HeuristicResult result) {
  if (heuristic_result_ == result) {
    return;
  }
  heuristic_result_ = result;

  if (!is_uma_recorded_) {
    uma_timer_.StartOneShot(kUMATimerDelay, FROM_HERE);
  }

  if (heuristic_result_ != HeuristicResult::kNeedsA11ySupport) {
    return;
  }
  // TODO(crbug.com/498093320): Trigger accessibility support.
  // TODO(crbug.com/475512055): Add UKM for cases that need a11y support for
  // verification.
}

void HTMLCanvasAccessibilityManager::RecordUma(TimerBase*) {
  if (is_uma_recorded_) {
    return;
  }
  is_uma_recorded_ = true;
  base::UmaHistogramEnumeration("Accessibility.Canvas.HeuristicResult",
                                heuristic_result_);
}

void HTMLCanvasAccessibilityManager::FlushUmaIfNeeded() {
  if (uma_timer_.IsActive()) {
    uma_timer_.Stop();
    RecordUma(nullptr);
  }
}

void HTMLCanvasAccessibilityManager::Trace(Visitor* visitor) const {
  visitor->Trace(uma_timer_);
  visitor->Trace(canvas_element_);
}

}  // namespace blink
