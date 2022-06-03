// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_
#define IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_

#import "ios/chrome/app/main_application_delegate.h"

@class MainController;
@class AppState;

@interface MainApplicationDelegate ()
@property(nonatomic, readonly) MainController* mainController;

+ (MainController*)sharedMainController;
+ (AppState*)sharedAppState;

@end
#endif  // IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_
