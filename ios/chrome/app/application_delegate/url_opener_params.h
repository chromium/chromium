// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_PARAMS_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_PARAMS_H_

#import <UIKit/UIKit.h>

// iOS has different ways to ask the application to open a URL. Each of these
// APIs use a different format for their parameter, but all use a URL and a
// source application.
// This class is a unification of the different formats of parameters.
@interface URLOpenerParams : NSObject

// The URL to open.
@property(copy, readonly) NSURL* URL;

// The external application that requested to open the URL.
@property(copy, readonly) NSString* sourceApplication;

- (instancetype)init NS_UNAVAILABLE;

// The init method to be used with explicit URL and sourceApplication.
- (instancetype)initWithURL:(NSURL*)URL
          sourceApplication:(NSString*)sourceApplication
    NS_DESIGNATED_INITIALIZER;

// The init method for the UIWindowSceneDelegate format.
- (instancetype)initWithUIOpenURLContext:(UIOpenURLContext*)context;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_PARAMS_H_
