// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_LAUNCH_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_APP_LAUNCH_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller that resembles the launch screen, with the Chrome logo at the
// center and Google branding at the bottom. Usually displays if the app
// foreground becomes active before Chrome UI is ready, so a smoother UI
// transition from the launch screen to Chrome could be presented to users.
@interface LaunchScreenViewController : UIViewController

// The view between the Chrome logo and the branding, ideally explaining why
// Chrome is not ready yet. For the detailView to be visible, it should be set
// in the subclass before the view is loaded.
@property(nonatomic, strong) UIView* detailView;

@end

#endif  // IOS_CHROME_APP_LAUNCH_SCREEN_VIEW_CONTROLLER_H_
