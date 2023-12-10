// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_RESPONSE_INFO_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_RESPONSE_INFO_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_support.h"
#include "ios/chrome/browser/overlays/model/public/overlay_user_data.h"

// Template for OverlayUserData used to create OverlayResponses.
template <class InfoType>
class OverlayResponseInfo : public OverlayUserData<InfoType> {
 public:
  // Returns an OverlayResponseSupport that only supports responses created with
  // InfoType.
  static const OverlayResponseSupport* ResponseSupport() {
    static base::NoDestructor<SupportsOverlayResponse<InfoType>> kSupport;
    return kSupport.get();
  }
};

// Macro used to define an OverlayResponseInfo that holds no data.  Should be
// declared in headers and accompanied by OVERLAY_USER_DATA_SETUP_IMPL() in the
// implementation.
#define DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(InfoType)  \
  class InfoType : public OverlayResponseInfo<InfoType> { \
   private:                                               \
    OVERLAY_USER_DATA_SETUP(InfoType);                    \
  }

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_RESPONSE_INFO_H_
