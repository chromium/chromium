// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_alert_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BOOL IsPriceAlertsEnabled() {
  // TODO(crbug.com/1245022): enable for MSBB users.
  // TODO(crbug.com/1245019): enable for feature flag.
  // TODO(crbug.com/1245022): pass in Webstate and disable for incognito.
  return false;
}
