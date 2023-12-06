// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_DEFAULT_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_DEFAULT_INFOBAR_OVERLAY_REQUEST_FACTORY_H_

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"

// Default InfobarOverlayRequestFactory that creates OverlayRequests according
// to the infobar type.
std::unique_ptr<OverlayRequest> DefaultInfobarOverlayRequestFactory(
    InfoBarIOS* infobar_ios,
    InfobarOverlayType overlay_type);

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_DEFAULT_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
