// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_

#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace mobile_metrics_test_helpers {

// Collect MobileFriendliness metrics with tree structure which reflects
// tree structure of subframe.
struct MobileFriendlinessTree {
  MobileFriendliness mf;
  WTF::Vector<MobileFriendlinessTree> children;

  static MobileFriendlinessTree GetMobileFriendlinessTree(
      LocalFrameView* view) {
    mobile_metrics_test_helpers::MobileFriendlinessTree result;
    view->UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kTest);
    view->GetMobileFriendlinessChecker()->NotifyFirstContentfulPaint();
    view->GetMobileFriendlinessChecker()->EvaluateNow();
    result.mf = view->GetMobileFriendlinessChecker()->GetMobileFriendliness();
    for (Frame* child = view->GetFrame().FirstChild(); child;
         child = child->NextSibling()) {
      if (LocalFrame* local_frame = DynamicTo<LocalFrame>(child)) {
        result.children.push_back(
            GetMobileFriendlinessTree(local_frame->View()));
      }
    }
    return result;
  }
};

}  // namespace mobile_metrics_test_helpers
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOBILE_METRICS_MOBILE_METRICS_TEST_HELPERS_H_
