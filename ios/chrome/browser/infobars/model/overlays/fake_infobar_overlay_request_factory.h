// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"

// Fake InfobarOverlayRequestFactory that only creates OverlayRequests that
// are configured with an InfobarOverlayRequestConfig.
std::unique_ptr<OverlayRequest> FakeInfobarOverlayRequestFactory(
    InfoBarIOS* infobar_ios,
    InfobarOverlayType overlay_type);

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
