// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_OVERLAY_TEST_MACROS_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_OVERLAY_TEST_MACROS_H_

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

// Macro used to define an OverlayRequestConfig that holds no data.  Can be used
// in tests for functionality specific to config types.
#define DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ConfigType) \
  DEFINE_STATELESS_OVERLAY_REQUEST_CONFIG(ConfigType); \
  OVERLAY_USER_DATA_SETUP_IMPL(ConfigType)

// Macro used to define a response info that holds no data.  Can be used
// in tests for functionality specific to info types.
#define DEFINE_TEST_OVERLAY_RESPONSE_INFO(InfoType) \
  DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(InfoType); \
  OVERLAY_USER_DATA_SETUP_IMPL(InfoType)

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_OVERLAY_TEST_MACROS_H_
