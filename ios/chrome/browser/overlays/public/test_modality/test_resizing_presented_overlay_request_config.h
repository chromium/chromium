// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_TEST_MODALITY_TEST_RESIZING_PRESENTED_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_TEST_MODALITY_TEST_RESIZING_PRESENTED_OVERLAY_REQUEST_CONFIG_H_

#import <QuartzCore/QuartzCore.h>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

// An OverlayRequestConfig to use in tests for presented UIViewControllers that
// resize their presentation container views.
class TestResizingPresentedOverlay
    : public OverlayRequestConfig<TestResizingPresentedOverlay> {
 public:
  ~TestResizingPresentedOverlay() override;

  // The frame of the presented view in window coordinates.
  const CGRect& frame() { return frame_; }

 private:
  OVERLAY_USER_DATA_SETUP(TestResizingPresentedOverlay);
  TestResizingPresentedOverlay(const CGRect& frame);

  const CGRect frame_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_TEST_MODALITY_TEST_RESIZING_PRESENTED_OVERLAY_REQUEST_CONFIG_H_
