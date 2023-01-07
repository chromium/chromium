// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_SHELL_SHELL_APP_DELEGATE_H_
#define IOS_WEB_VIEW_SHELL_SHELL_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// UIApplicationDelegate implementation for web_view_shell.
@interface ShellAppDelegate : UIResponder<UIApplicationDelegate>

// The main window for the application.
@property(nonatomic, strong) UIWindow* window;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_APP_DELEGATE_H_
