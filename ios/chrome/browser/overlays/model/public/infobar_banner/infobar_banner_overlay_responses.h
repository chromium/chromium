// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_

#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

// Response info used to create dispatched OverlayResponses that trigger the
// infobar's main action.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(InfobarBannerMainActionResponse);

// Response info used to create dispatched OverlayResponses that trigger the
// presentation of the infobar's modal.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(InfobarBannerShowModalResponse);

// Response info used to create dispatched OverlayResponses that notify the
// model layer that the upcoming dismissal is user-initiated (i.e. swipe up to
// dismiss the banner on the refresh banner UI).
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(
    InfobarBannerUserInitiatedDismissalResponse);

// Response info used to create dispatched OverlayResponses that notify the
// model layer that the request's Infobar should be removed.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(InfobarBannerRemoveInfobarResponse);

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_RESPONSES_H_
