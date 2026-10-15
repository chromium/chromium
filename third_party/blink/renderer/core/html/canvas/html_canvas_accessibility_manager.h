// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class HTMLCanvasElement;

// HTMLCanvasAccessibilityManager contains the accessibility information of the
// canvas element, determines if the canvas element needs accessibility support,
// and adds the support if needed.
class CORE_EXPORT HTMLCanvasAccessibilityManager
    : public GarbageCollected<HTMLCanvasAccessibilityManager> {
 public:
  // TODO(crbug.com/498093320): Add A/B testing for below constants. All
  // constants are chosen ad hoc and should be fine tuned.

  // Two rectangles are considered to be sufficiently overlapping if the
  // intersection area is greater than 80% of the area of either rectangle.
  static constexpr float kSufficientlyOverlappingThreshold = 0.8f;

  // Two texts are considered to be on the same line if the vertical distance
  // between them is less than 10% of the size of the smaller font.
  static constexpr float kSameLineFontRatioThreshold = 0.1f;

  // Delay to ensure the canvas element has had a chance to update its
  // accessibility related information.
  static constexpr base::TimeDelta kUMATimerDelay = base::Seconds(5);

  // Delay before triggering OCR after the canvas image becomes stable.
  static constexpr base::TimeDelta kOCRDelay = base::Seconds(1);

  // Maximum time an OCR request can be postponed before rescheduling ceases.
  static constexpr base::TimeDelta kMaxOCRDelay = base::Seconds(5);

  // Threshold of text runs count beyond which too much text is considered
  // extracted, requiring support from the OCR service.
  static constexpr size_t kMaxTextRuns = 10;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(CanvasAccessibilityHeuristicResult)
  enum class HeuristicResult {
    kUnknown,
    kDeprecatedIsNotVisible,  // Not used anymore.
    kIsIgnored,
    kTooSmall,
    kHasContentDrawable,
    kHasFallbackContent,
    kHasAriaAttributes,
    kNeedsA11ySupport,
    kMaxValue = kNeedsA11ySupport,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:CanvasAccessibilityHeuristicResult)

  // `canvas_element` should outlive this object and should be the owner of it.
  HTMLCanvasAccessibilityManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::optional<bool> is_ignored,
      HTMLCanvasElement* canvas_element);
  HTMLCanvasAccessibilityManager(const HTMLCanvasAccessibilityManager&) =
      delete;
  HTMLCanvasAccessibilityManager& operator=(
      const HTMLCanvasAccessibilityManager&) = delete;

  // Pure stateless heuristic computation.
  static HeuristicResult ComputeHeuristicResult(
      const HTMLCanvasElement& canvas_element,
      bool is_ignored);

  bool NeedsA11ySupport() const {
    return heuristic_result_ == HeuristicResult::kNeedsA11ySupport;
  }

  std::optional<bool> GetIsIgnored() const { return is_ignored_; }

  void Trace(Visitor* visitor) const;

  HeuristicResult GetHeuristicResultForTesting() const {
    return heuristic_result_;
  }

  void SetIsIgnored(bool is_ignored);

  void OnResize();

  void OnUpdate();

  bool ShouldCaptureRenderedText() const;

  void RecordRenderedText(const String& text,
                          const gfx::RectF& bounds,
                          float font_height);
  void ClearRenderedText(const gfx::RectF& rect);
  void ClearRenderedText();
  const String& CanvasAnnotation() const { return canvas_annotation_; }
  void UpdateAnnotation();

  bool HasRequestedOCR() const { return has_requested_ocr_; }
  void ClearHasRequestedOCR() { has_requested_ocr_ = false; }

  // Records the heuristic result to UMA if it hasn't been recorded yet. UMA is
  // recorded as a best effort in a timer to let the canvas element update its
  // accessibility related information. If it is not recorded by the time the
  // canvas element's context is destroyed, it will be recorded then.
  void FlushUmaIfNeeded();

  bool ShouldCaptureRenderedTextForTesting() const {
    return ShouldCaptureRenderedText();
  }

  bool IsOCRTimerActiveForTesting() const { return ocr_timer_.IsActive(); }

 private:
  void SetHeuristicResult(HeuristicResult result);
  void RecordUma(TimerBase*);
  static bool IsTooSmall(const HTMLCanvasElement& canvas_element);
  bool ShouldRunOCR() const;
  void ScheduleOCRIfNeeded();
  void TriggerOCR(TimerBase*);
  void SetOCRDeadline();
  void ClearOCRDeadline();

  struct RenderedTextRun {
    String text;
    gfx::RectF bounds;
    float font_height;
  };

  std::optional<bool> is_ignored_;
  HeuristicResult heuristic_result_ = HeuristicResult::kUnknown;

  HeapTaskRunnerTimer<HTMLCanvasAccessibilityManager> uma_timer_;
  HeapTaskRunnerTimer<HTMLCanvasAccessibilityManager> ocr_timer_;
  base::TimeTicks ocr_deadline_;
  bool is_uma_recorded_ = false;

  // Owns this object and should outlive it.
  Member<HTMLCanvasElement> canvas_element_;

  Vector<RenderedTextRun> text_runs_;
  String canvas_annotation_;
  bool has_requested_ocr_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ACCESSIBILITY_MANAGER_H_
