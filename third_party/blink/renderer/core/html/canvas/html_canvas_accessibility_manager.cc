// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/accessibility_features.h"

namespace blink {

namespace {

bool SufficientlyOverlapping(const gfx::RectF& a, const gfx::RectF& b) {
  gfx::RectF intersection = gfx::IntersectRects(a, b);
  if (intersection.IsEmpty()) {
    return false;
  }
  float intersection_area = intersection.size().GetArea();
  float a_area = a.size().GetArea();
  float b_area = b.size().GetArea();
  CHECK_GT(a_area, 0);
  CHECK_GT(b_area, 0);
  return (intersection_area / a_area >
          HTMLCanvasAccessibilityManager::kSufficientlyOverlappingThreshold) ||
         (intersection_area / b_area >
          HTMLCanvasAccessibilityManager::kSufficientlyOverlappingThreshold);
}

}  // namespace

HTMLCanvasAccessibilityManager::HTMLCanvasAccessibilityManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_ignored,
    HTMLCanvasElement* canvas_element,
    Options options)
    : options_(options),
      is_ignored_(is_ignored),
      uma_timer_(task_runner, this, &HTMLCanvasAccessibilityManager::RecordUma),
      ocr_timer_(std::move(task_runner),
                 this,
                 &HTMLCanvasAccessibilityManager::TriggerOCR),
      canvas_element_(canvas_element) {
  UpdateHasFallbackElementContent();

  has_layoutsubtree_ = canvas_element->layoutSubtree();

  ReadAriaAttributes();

  // If the manager is created with the kCollectTextRuns option only, capture
  // the rendered text until heuristic decides otherwise.
  if (options_ == kCollectTextRuns) {
    canvas_element_->UpdateCaptureRenderedText(true);
    return;
  }

  if (options_ & kUpdateHeuristicResults) {
    OnUpdate();
  }
}

void HTMLCanvasAccessibilityManager::SetIsIgnored(bool is_ignored) {
  // If AX tree annotation is already enabled and `is_ignored` state is not
  // changed, update is not needed.
  if ((options_ & kAnnotateAXTree) && is_ignored_ == is_ignored) {
    return;
  }

  // Receiving `is_ignored` indicates that AXObject is created for the canvas,
  // and the manager should go to full operation mode to decide if the canvas
  // needs accessibility support or not, and support it if needed.
  // Note that support is not needed if `is_ignored` is true, however the
  // update flow is triggered to update the heuristic result and collect
  // metrics.
  options_ |= kAll;
  is_ignored_ = is_ignored;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::EnableUpdatingHeuristicResults() {
  if (options_ & kUpdateHeuristicResults) {
    return;
  }
  options_ |= kUpdateHeuristicResults;
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
  if (is_ignored_) {
    // If the canvas is ignored, accessibility support is not needed and we
    // don't need to check for aria attributes.
    return;
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

void HTMLCanvasAccessibilityManager::SetHasLayoutSubtree(
    bool has_layoutsubtree) {
  if (has_layoutsubtree_ == has_layoutsubtree) {
    return;
  }
  has_layoutsubtree_ = has_layoutsubtree;
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnResize() {
  ClearRenderedText();
  OnUpdate();
}

void HTMLCanvasAccessibilityManager::OnUpdate() {
  if (!(options_ & kUpdateHeuristicResults)) {
    return;
  }

  // If AX ignores the canvas, it definitely doesn't need support.
  // Note that covers elements with aria-hidden=true, or not visible, or 1x1
  // pixel size are ignored.
  if (is_ignored_) {
    SetHeuristicResult(HeuristicResult::kIsIgnored);
    return;
  }

  // Minimum size canvas to assume it can have text.
  if (IsTooSmall()) {
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

bool HTMLCanvasAccessibilityManager::IsTooSmall() const {
  const float kMinPixelSizeForTextVisibility = 10;
  gfx::SizeF layout_size;
  if (auto* layout_box =
          DynamicTo<LayoutBox>(canvas_element_->GetLayoutObject())) {
    layout_size.set_width(layout_box->LogicalWidth().ToFloat());
    layout_size.set_height(layout_box->LogicalHeight().ToFloat());
  }
  return layout_size.width() < kMinPixelSizeForTextVisibility ||
         layout_size.height() < kMinPixelSizeForTextVisibility;
}

void HTMLCanvasAccessibilityManager::SetHeuristicResult(
    HeuristicResult result) {
  if (heuristic_result_ == result) {
    return;
  }
  heuristic_result_ = result;

  if (!(options_ & kAnnotateAXTree)) {
    return;
  }

  if (!is_uma_recorded_) {
    uma_timer_.StartOneShot(HTMLCanvasAccessibilityManager::kUMATimerDelay,
                            FROM_HERE);
  }

  if (heuristic_result_ != HeuristicResult::kNeedsA11ySupport) {
    ocr_timer_.Stop();
    ClearOCRDeadline();
  }

  if (heuristic_result_ == HeuristicResult::kNeedsA11ySupport &&
      base::FeatureList::IsEnabled(::features::kAccessibilityCanvas)) {
    options_ |= kCollectTextRuns;
    // Schedule OCR to ensure static canvases are annotated if conditions
    // change later.
    ScheduleOCRIfNeeded();
  } else if (options_ & kCollectTextRuns) {
    ClearRenderedText();
    options_ &= ~kCollectTextRuns;
  }
  canvas_element_->UpdateCaptureRenderedText(options_ & kCollectTextRuns);
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

void HTMLCanvasAccessibilityManager::RecordRenderedText(
    const String& text,
    const gfx::RectF& bounds,
    float font_height) {
  if (!(options_ & kCollectTextRuns)) {
    return;
  }

  // No need to capture empty text or bounds, or zero font height.
  if (font_height == 0 || text.empty() || bounds.IsEmpty()) {
    return;
  }

  // Check for overwrite.
  for (auto& run : text_runs_) {
    if (SufficientlyOverlapping(run.bounds, bounds)) {
      run.text = text;
      run.font_height = font_height;
      return;
    }
  }
  text_runs_.push_back(RenderedTextRun{text, bounds, font_height});
}

void HTMLCanvasAccessibilityManager::ClearRenderedText(const gfx::RectF& rect) {
  if (!(options_ & kCollectTextRuns)) {
    return;
  }

  // Remove runs that are mostly inside `rect`
  EraseIf(text_runs_, [&rect](const RenderedTextRun& run) {
    gfx::RectF intersection = gfx::IntersectRects(run.bounds, rect);
    if (intersection.IsEmpty()) {
      return false;
    }
    float run_area = run.bounds.size().GetArea();
    if (run_area == 0) {
      return true;
    }
    return (intersection.size().GetArea() / run_area) >
           HTMLCanvasAccessibilityManager::kSufficientlyOverlappingThreshold;
  });
}

void HTMLCanvasAccessibilityManager::ClearRenderedText() {
  text_runs_.clear();
}

void HTMLCanvasAccessibilityManager::UpdateAnnotation() {
  if (text_runs_.empty()) {
    canvas_annotation_ = String();
  } else {
    // Sort runs: Y (top to bottom), then X (left to right)
    // TODO(crbug.com/498093320): Add language direction (LTR/RTL) to TextRuns
    // tests.
    Vector<RenderedTextRun> sorted_runs = text_runs_;
    std::sort(sorted_runs.begin(), sorted_runs.end(),
              [](const RenderedTextRun& a, const RenderedTextRun& b) {
                float y_diff = a.bounds.y() - b.bounds.y();
                float same_line_threshold =
                    std::min(a.font_height, b.font_height) *
                    HTMLCanvasAccessibilityManager::kSameLineFontRatioThreshold;
                if (std::abs(y_diff) < same_line_threshold) {
                  return a.bounds.x() < b.bounds.x();
                }
                return a.bounds.y() < b.bounds.y();
              });

    StringBuilder builder;
    for (const auto& run : sorted_runs) {
      if (!builder.empty()) {
        // TODO(crbug.com/498093320): This may add space in the middle of words
        // as there can be cases where a word is split into multiple runs.
        // Consider using font height to determine if the gap is large enough to
        // require a space.
        builder.Append(' ');
      }
      builder.Append(run.text);
    }
    canvas_annotation_ = builder.ToString();
  }

  if (AXObjectCache* cache =
          canvas_element_->GetDocument().ExistingAXObjectCache()) {
    cache->MarkElementDirty(canvas_element_);
  }

  ScheduleOCRIfNeeded();
}

void HTMLCanvasAccessibilityManager::ClearOCRDeadline() {
  // Reset the deadline to null.
  ocr_deadline_ = base::TimeTicks();
}

void HTMLCanvasAccessibilityManager::SetOCRDeadline() {
  // Set the deadline to `kMaxOCRDelay` after now to ensure that OCR is not
  // rescheduled beyond the deadline.
  ocr_deadline_ =
      base::TimeTicks::Now() + HTMLCanvasAccessibilityManager::kMaxOCRDelay;
}

bool HTMLCanvasAccessibilityManager::ShouldRunOCR() const {
  if (features::GetCanvasAccessibilityMode() !=
      features::CanvasAccessibilityMode::kAdvanced) {
    return false;
  }
  if (!NeedsA11ySupport()) {
    return false;
  }
  // OCR is needed if no text is extracted, or too much text is extracted.
  // TODO(crbug.com/498093320): Add histogram metrics on OCR results to help
  // planning for threshold updates, including:
  //  - How much text gets extracted from canvases with zero text runs?
  //  - For various ranges of text run size, how different is the OCR'ed text
  //    from the canvas annotation?
  //  - What percentage of canvases that need OCR never get OCR'ed? (e.g. due to
  //    the timer getting reset constantly.)
  return text_runs_.empty() ||
         text_runs_.size() > HTMLCanvasAccessibilityManager::kMaxTextRuns;
}

void HTMLCanvasAccessibilityManager::TriggerOCR(TimerBase*) {
  ClearOCRDeadline();
  has_requested_ocr_ = true;
  if (AXObjectCache* cache =
          canvas_element_->GetDocument().ExistingAXObjectCache()) {
    // Marking the element dirty forces an accessibility tree serialization
    // update, during which AXCanvasAnnotator will see the dirty state and
    // invoke the Screen AI OCR service.
    cache->MarkElementDirty(canvas_element_);
  }
}

void HTMLCanvasAccessibilityManager::ScheduleOCRIfNeeded() {
  if (ShouldRunOCR()) {
    if (!ocr_timer_.IsActive()) {
      SetOCRDeadline();
      ocr_timer_.StartOneShot(HTMLCanvasAccessibilityManager::kOCRDelay,
                              FROM_HERE);
    } else {
      if (base::TimeTicks::Now() < ocr_deadline_) {
        ocr_timer_.StartOneShot(HTMLCanvasAccessibilityManager::kOCRDelay,
                                FROM_HERE);
      }
    }
  } else {
    ocr_timer_.Stop();
    ClearOCRDeadline();
  }
}

void HTMLCanvasAccessibilityManager::Trace(Visitor* visitor) const {
  visitor->Trace(uma_timer_);
  visitor->Trace(ocr_timer_);
  visitor->Trace(canvas_element_);
}

}  // namespace blink
