// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"

@protocol AppBarMutator;

// View controller for the app bar.
@interface AppBarViewController : UIViewController <AppBarConsumer>

// The mutator.
@property(nonatomic, weak) id<AppBarMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
