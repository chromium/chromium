// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_

#include <memory>

#include "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"

class InfoBarIOS;
class OverlayRequest;

// Type of a factory function that converts InfoBars into OverlayRequests.
using InfobarOverlayRequestFactory =
    std::unique_ptr<OverlayRequest> (*)(InfoBarIOS* infobar_ios,
                                        InfobarOverlayType overlay_type);

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_H_
