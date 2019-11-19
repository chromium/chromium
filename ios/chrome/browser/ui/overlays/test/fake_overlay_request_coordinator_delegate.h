// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"

#include <map>

// Fake implementation of OverlayRequestCoordinatorDelegate.
class FakeOverlayRequestCoordinatorDelegate
    : public OverlayRequestCoordinatorDelegate {
 public:
  FakeOverlayRequestCoordinatorDelegate();
  ~FakeOverlayRequestCoordinatorDelegate() override;

  // Whether the overlay UI for |request| has been presented.
  bool HasUIBeenPresented(OverlayRequest* request) const;

  // Whether the overlay UI for |request| has been dismissed.
  bool HasUIBeenDismissed(OverlayRequest* request) const;

  // OverlayRequestCoordinatorDelegate:
  void OverlayUIDidFinishPresentation(OverlayRequest* request) override;
  void OverlayUIDidFinishDismissal(OverlayRequest* request) override;

 private:
  enum class PresentationState { kNotPresented, kPresented, kDismissed };
  std::map<OverlayRequest*, PresentationState> states_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
