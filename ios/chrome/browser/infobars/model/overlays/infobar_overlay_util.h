// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_UTIL_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_UTIL_H_

#include <stddef.h>

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"

class InfoBarIOS;
class OverlayRequest;
class OverlayRequestQueue;

// Returns the InfoBarIOS used to configure `request`, or null if the InfoBarIOS
// was already destroyed or if `request` was not created with an infobar config.
InfoBarIOS* GetOverlayRequestInfobar(OverlayRequest* request);

// Returns the InfobarType of the InfoBar used to configure `request`.
// `request` must be non-null and configured with an
// InfobarOverlayRequestConfig.
// TODO(crbug.com/40113384): Remove requirements on `request` and return
// InfobarType::kNone once added.
InfobarType GetOverlayRequestInfobarType(OverlayRequest* request);

// Returns the InfobarOverlayType for `request`.  `request` must be non-null and
// configured with an InfobarOverlayRequestConfig.
InfobarOverlayType GetOverlayRequestInfobarOverlayType(OverlayRequest* request);

// Searches through `queue` for an OverlayRequest configured with `infobar`.  If
// found, returns true and populates `index` with the index of the first request
// configured with `infobar`.  If no matching request was found, returns false.
// All arguments must be non-null.
bool GetInfobarOverlayRequestIndex(OverlayRequestQueue* queue,
                                   InfoBarIOS* infobar,
                                   size_t* index);

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_UTIL_H_
