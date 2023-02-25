// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_IOS_SHELL_APPLICATION_IOS_H_
#define CONTENT_SHELL_APP_IOS_SHELL_APPLICATION_IOS_H_

#ifdef __OBJC__
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// UIApplicationDelegate implementation for web_view_shell.
@interface ShellAppDelegate : UIResponder <UIApplicationDelegate>

@end

NS_ASSUME_NONNULL_END
#endif

int RunShellApplication(int argc, const char* _Nullable* _Nullable argv);

#endif  // CONTENT_SHELL_APP_IOS_SHELL_APPLICATION_IOS_H_
