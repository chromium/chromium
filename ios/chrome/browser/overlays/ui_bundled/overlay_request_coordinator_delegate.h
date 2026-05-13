// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"

// Delegate class used to communicate overlay UI presentation events back to
// OverlayPresenter.
class OverlayRequestCoordinatorDelegate {
 public:
  OverlayRequestCoordinatorDelegate() = default;
  virtual ~OverlayRequestCoordinatorDelegate() = default;

  // Called to notify the delegate that the UI for `request` has finished being
  // presented.
  virtual void OverlayUIDidFinishPresentation(OverlayRequest* request) = 0;

  // Called to notify the delegate that the UI for the request with `request_id`
  // has finished being dismissed.
  // NOTE: This method accepts `OverlayRequestId` rather
  // than a raw `OverlayRequest*` pointer to avoid UAF. The OverlayRequest
  // object might have already been deleted when the method is called.
  virtual void OverlayUIDidFinishDismissal(OverlayRequestId request_id) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_DELEGATE_H_
