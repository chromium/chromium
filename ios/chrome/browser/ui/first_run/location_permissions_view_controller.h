// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// Presents a fullscreen modal that explains location data usage. Accepting the
// main action will trigger a native location permissions prompt.
@interface LocationPermissionsViewController : ConfirmationAlertViewController
@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_VIEW_CONTROLLER_H_
