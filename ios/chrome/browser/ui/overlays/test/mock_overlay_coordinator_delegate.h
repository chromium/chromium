// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MOCK_OVERLAY_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MOCK_OVERLAY_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mock version of OverlayRequestCoordinatorDelegate.
class MockOverlayRequestCoordinatorDelegate
    : public OverlayRequestCoordinatorDelegate {
 public:
  MockOverlayRequestCoordinatorDelegate();
  ~MockOverlayRequestCoordinatorDelegate();

  MOCK_METHOD1(OverlayUIDidFinishPresentation, void(OverlayRequest* request));
  MOCK_METHOD1(OverlayUIDidFinishDismissal, void(OverlayRequest* request));
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_MOCK_OVERLAY_COORDINATOR_DELEGATE_H_
