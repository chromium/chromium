// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_

class OverlayRequest;

// Delegate class used to communicate overlay UI presentation events back to
// OverlayPresenter.
class OverlayRequestCoordinatorDelegate {
 public:
  OverlayRequestCoordinatorDelegate() = default;
  virtual ~OverlayRequestCoordinatorDelegate() = default;

  // Called to notify the delegate that the UI for |request| has finished being
  // presented.
  virtual void OverlayUIDidFinishPresentation(OverlayRequest* request) = 0;

  // Called to notify the delegate that the UI for |request| is finished
  // being dismissed.
  virtual void OverlayUIDidFinishDismissal(OverlayRequest* request) = 0;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
