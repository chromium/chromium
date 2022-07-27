// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Document;
class LocalFrameView;
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
  static MobileFriendlinessChecker* From(const Document&);

  // LocalFrameView::LifecycleNotificationObserver implementation
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;
  void NotifyInitialScaleUpdated();

  void NotifyPaintBegin();
  void NotifyPaintEnd();
  void WillBeRemovedFromFrame();
  void NotifyViewportUpdated(const ViewportDescription&);
  void NotifyPaintTextFragment(const PhysicalRect& paint_rect, int font_size);
  void NotifyPaintReplaced(const PhysicalRect& paint_rect);

  void Trace(Visitor* visitor) const override;

  struct AreaSizes {
    double small_font_area = 0;
    double total_text_area = 0;
    double content_beyond_viewport_area = 0;
    int TextContentsOutsideViewportPercentage(double viewport_area) const;
    int SmallTextRatio() const;
  };

  class PaintScope final {
    STACK_ALLOCATED();

   public:
    explicit PaintScope(MobileFriendlinessChecker& mfc) : mfc_(mfc) {
      mfc_.NotifyPaintBegin();
    }
    ~PaintScope() { mfc_.NotifyPaintEnd(); }

   private:
    MobileFriendlinessChecker& mfc_;
  };

  class IgnoreBeyondViewportScope final {
    STACK_ALLOCATED();

   public:
    explicit IgnoreBeyondViewportScope(MobileFriendlinessChecker& mfc)
        : mfc_(mfc) {
      mfc_.ignore_beyond_viewport_scope_count_++;
    }
    ~IgnoreBeyondViewportScope() { mfc_.ignore_beyond_viewport_scope_count_--; }

   private:
    MobileFriendlinessChecker& mfc_;
  };

 private:
  void Activate(TimerBase*);

  // Returns percentage value [0-100] of bad tap targets in the area of the
  // first page. Returns kTimeBudgetExceeded if the time limit is exceeded.
  int ComputeBadTapTargetsRatio();

  void UpdateTextAreaSizes(const PhysicalRect& text_rect, int font_size);
  void UpdateBeyondViewportAreaSizes(const PhysicalRect& paint_rect);

 private:
  Member<LocalFrameView> frame_view_;
  HeapTaskRunnerTimer<MobileFriendlinessChecker> timer_;
  double viewport_scalar_;
  double initial_scale_ = 1.0;
  base::TimeTicks last_evaluated_;
  AreaSizes area_sizes_;
  bool viewport_device_width_ = false;
  bool allow_user_zoom_ = true;
  int viewport_initial_scale_x10_ = -1;
  int viewport_hardcoded_width_ = -1;
  int ignore_beyond_viewport_scope_count_ = 0;
  bool is_painting_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
