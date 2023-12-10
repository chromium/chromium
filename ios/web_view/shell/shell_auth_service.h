// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_SHELL_SHELL_AUTH_SERVICE_H_
#define IOS_WEB_VIEW_SHELL_SHELL_AUTH_SERVICE_H_

#import <ChromeWebView/ChromeWebView.h>

NS_ASSUME_NONNULL_BEGIN

// Authorization service for ios_web_view_shell.
@interface ShellAuthService : NSObject<CWVSyncControllerDataSource>

// Returns available identities.
- (NSArray<CWVIdentity*>*)identities;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_AUTH_SERVICE_H_
