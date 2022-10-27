// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_ENTERPRISE_LOADING_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_APP_ENTERPRISE_LOADING_SCREEN_VIEW_CONTROLLER_H_

#import "ios/chrome/app/launch_screen_view_controller.h"

// View controller that is displayed to users when waiting in enterprise stage.
// It contains a loading spinner and text underneath, explaining that the
// browser is managed.
@interface EnterpriseLoadScreenViewController : LaunchScreenViewController

@end

#endif  // IOS_CHROME_APP_ENTERPRISE_LOADING_SCREEN_VIEW_CONTROLLER_H_
