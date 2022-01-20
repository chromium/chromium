// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class LocalFrameView;
class LayoutObject;

struct ViewportDescription;

// Calculates the mobile usability of current page, especially friendliness on
// smart phone devices are checked. The calculated value will be sent as a part
// of UKM.
class CORE_EXPORT MobileFriendlinessChecker
    : public GarbageCollected<MobileFriendlinessChecker>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  explicit MobileFriendlinessChecker(LocalFrameView& frame_view);
  virtual ~MobileFriendlinessChecker();

  // LocalFrameView::LifecycleNotificationObserver implementation
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  void NotifyPaint();
  void WillBeRemovedFromFrame();
  void NotifyViewportUpdated(const ViewportDescription&);
  void NotifyInvalidatePaint(const LayoutObject& object);
  const blink::MobileFriendliness& GetMobileFriendliness() const {
    return mobile_friendliness_;
  }

  void Trace(Visitor* visitor) const override;
  struct TextAreaWithFontSize {
    double small_font_area = 0;
    double total_text_area = 0;
    int SmallTextRatio() const;
  };

 private:
  void Activate(TimerBase*);

  // Returns the percentage of the width of the content that overflows the
  // viewport.
  // Returns 0 if all content fits in the viewport.
  int ComputeContentOutsideViewport();

  // Returns percentage value [0-100] of bad tap targets in the area of the
  // first page. Returns kTimeBudgetExceeded if the time limit is exceeded.
  int ComputeBadTapTargetsRatio();

 private:
  TextAreaWithFontSize text_area_sizes_;
  Member<LocalFrameView> frame_view_;
  MobileFriendliness mobile_friendliness_;
  HeapTaskRunnerTimer<MobileFriendlinessChecker> timer_;
  base::TimeTicks last_evaluated_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
