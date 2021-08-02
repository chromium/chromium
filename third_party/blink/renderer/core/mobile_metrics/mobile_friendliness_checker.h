// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class LocalFrameView;
class LayoutObject;

struct ViewportDescription;

// Calculates the mobile usability of current page, especially friendliness on
// smart phone devices are checked. The calculated value will be sent as a part
// of UKM.
class CORE_EXPORT MobileFriendlinessChecker
    : public GarbageCollected<MobileFriendlinessChecker> {
 public:
  explicit MobileFriendlinessChecker(LocalFrameView& frame_view);
  virtual ~MobileFriendlinessChecker();

  void NotifyFirstContentfulPaint();
  void NotifyViewportUpdated(const ViewportDescription&);
  void NotifyInvalidatePaint(const LayoutObject& object);
  const blink::MobileFriendliness& GetMobileFriendliness() const {
    return mobile_friendliness_;
  }
  void EvaluateNow();

  void Trace(Visitor* visitor) const;
  struct TextAreaWithFontSize {
    double small_font_area = 0;
    double total_text_area = 0;
    int SmallTextRatio() const;
  };

 private:
  void ComputeSmallTextRatio(const LayoutObject& object);
  int ComputeContentOutsideViewport();
  void ComputeBadTapTargetsRatio();

 private:
  TextAreaWithFontSize text_area_sizes_;
  Member<LocalFrameView> frame_view_;
  blink::MobileFriendliness mobile_friendliness_;
  bool font_size_check_enabled_;
  bool tap_target_check_enabled_;
  float viewport_scalar_;
  bool fcp_detected_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
