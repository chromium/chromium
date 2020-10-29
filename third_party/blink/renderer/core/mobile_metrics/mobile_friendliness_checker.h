// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class Visitor;
class LocalFrameView;
struct ViewportDescription;

// Calculates the mobile usability of current page, especially friendliness on
// smart phone devices are checked. The calculated value will be sent as a part
// of UKM.
class CORE_EXPORT MobileFriendlinessChecker
    : public GarbageCollected<MobileFriendlinessChecker> {
 public:
  explicit MobileFriendlinessChecker(LocalFrameView& frame_view);
  virtual ~MobileFriendlinessChecker();

  void NotifyViewportUpdated(const ViewportDescription&);
  const blink::MobileFriendliness& GetMobileFriendliness() const {
    return mobile_friendliness_;
  }
  void Trace(Visitor* visitor) const;

 private:
  Member<LocalFrameView> frame_view_;
  blink::MobileFriendliness mobile_friendliness_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_FRIENDLINESS_CHECKER_H_
