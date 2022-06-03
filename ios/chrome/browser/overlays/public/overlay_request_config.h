// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CONFIG_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

// Template for OverlayUserData used to configure OverlayRequests.
template <class ConfigType>
class OverlayRequestConfig : public OverlayUserData<ConfigType> {
 public:
  // Returns an OverlayRequestSupport that only supports requests created with
  // ConfigType.
  static const OverlayRequestSupport* RequestSupport() {
    static base::NoDestructor<SupportsOverlayRequest<ConfigType>> kSupport;
    return kSupport.get();
  }
};

// Macro used to define an OverlayRequestConfig that holds no data.  Should be
// declared in headers and accompanied by OVERLAY_USER_DATA_SETUP_IMPL() in the
// implementation.
#define DEFINE_STATELESS_OVERLAY_REQUEST_CONFIG(ConfigType)    \
  class ConfigType : public OverlayRequestConfig<ConfigType> { \
   private:                                                    \
    OVERLAY_USER_DATA_SETUP(ConfigType);                       \
  }

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CONFIG_H_
