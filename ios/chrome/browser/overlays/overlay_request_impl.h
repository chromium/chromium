// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_

#include <memory>

#include "ios/chrome/browser/overlays/public/overlay_request.h"

// Internal implementation of OverlayRequest.
class OverlayRequestImpl : public OverlayRequest,
                           public base::SupportsUserData {
 public:
  OverlayRequestImpl();
  ~OverlayRequestImpl() override;

  // OverlayRequest:
  void set_response(std::unique_ptr<OverlayResponse> response) override;
  OverlayResponse* response() const override;
  void set_callback(OverlayCallback callback) override;
  base::SupportsUserData* data() override;

 private:
  // The response containing the user interaction information for the overlay
  // resulting from this response.
  std::unique_ptr<OverlayResponse> response_;
  // The callback to be executed upon dismissal of the overlay.
  OverlayCallback callback_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
