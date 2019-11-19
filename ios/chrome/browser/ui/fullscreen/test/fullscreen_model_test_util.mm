// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void SetUpFullscreenModelForTesting(FullscreenModel* model,
                                    CGFloat toolbar_height) {
  EXPECT_GE(toolbar_height, 0.0);
  model->SetCollapsedToolbarHeight(0.0);
  model->SetExpandedToolbarHeight(toolbar_height);
  model->SetScrollViewHeight(2 * toolbar_height);
  model->SetContentHeight(2 * model->GetScrollViewHeight());
  model->ResetForNavigation();
  model->SetYContentOffset(0.0);
}

void SimulateFullscreenUserScrollWithDelta(FullscreenModel* model,
                                           CGFloat offset_delta) {
  model->SetScrollViewIsDragging(true);
  model->SetScrollViewIsScrolling(true);
  model->SetYContentOffset(model->GetYContentOffset() + offset_delta);
  model->SetScrollViewIsDragging(false);
  model->SetScrollViewIsScrolling(false);
}

void SimulateFullscreenUserScrollForProgress(FullscreenModel* model,
                                             CGFloat progress) {
  ASSERT_GE(progress, 0.0);
  ASSERT_LE(progress, 1.0);
  SimulateFullscreenUserScrollWithDelta(
      model, GetFullscreenOffsetDeltaForProgress(model, progress));
}

CGFloat GetFullscreenOffsetDeltaForProgress(FullscreenModel* model,
                                            CGFloat progress) {
  CGFloat height_delta = model->toolbar_height_delta();
  CGFloat base_offset =
      GetFullscreenBaseOffsetForProgress(model, model->progress());
  CGFloat final_y_content_offset =
      base_offset + (1.0 - progress) * height_delta;
  return final_y_content_offset - model->GetYContentOffset();
}

CGFloat GetFullscreenBaseOffsetForProgress(FullscreenModel* model,
                                           CGFloat progress) {
  return model->GetYContentOffset() -
         (1.0 - progress) * model->toolbar_height_delta();
}
