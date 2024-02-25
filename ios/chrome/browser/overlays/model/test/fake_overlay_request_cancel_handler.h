// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"

// Fake cancel handler for use in tests that can be manually triggered.
class FakeOverlayRequestCancelHandler : public OverlayRequestCancelHandler {
 public:
  FakeOverlayRequestCancelHandler(OverlayRequest* request,
                                  OverlayRequestQueue* queue);
  ~FakeOverlayRequestCancelHandler() override;

  // Cancels the associated request.
  void TriggerCancellation();
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_REQUEST_CANCEL_HANDLER_H_
