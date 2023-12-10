// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_COMMON_PLACEHOLDER_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_COMMON_PLACEHOLDER_REQUEST_CONFIG_H_

#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"

// Placeholder request config that has no associated UI. It can be used to
// prevent the presentation of overlay UI for subsequent queued requests until
// an event finishes.
DEFINE_STATELESS_OVERLAY_REQUEST_CONFIG(PlaceholderRequestConfig);

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_COMMON_PLACEHOLDER_REQUEST_CONFIG_H_
