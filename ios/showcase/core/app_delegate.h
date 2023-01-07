// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_CORE_APP_DELEGATE_H_
#define IOS_SHOWCASE_CORE_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder<UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;

// Sets the application to display the Showcase home.
- (void)setupUI;

@end

#endif  // IOS_SHOWCASE_CORE_APP_DELEGATE_H_
