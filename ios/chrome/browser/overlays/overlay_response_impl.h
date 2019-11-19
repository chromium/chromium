// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_RESPONSE_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_RESPONSE_IMPL_H_

#include "ios/chrome/browser/overlays/public/overlay_response.h"

// Internal implementation of OverlayResponse.
class OverlayResponseImpl : public OverlayResponse,
                            public base::SupportsUserData {
 public:
  OverlayResponseImpl();
  ~OverlayResponseImpl() override;

 private:
  // OverlayResponse:
  base::SupportsUserData* data() override;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_RESPONSE_IMPL_H_
