// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_
#define IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_

#import "ios/chrome/app/main_application_delegate.h"

@class AppState;
@class MainController;

@interface MainApplicationDelegate ()
@property(nonatomic, class, readonly) MainController* sharedMainController;
@property(nonatomic, class, readonly) AppState* sharedAppState;
@property(nonatomic, readonly) MainController* mainController;
@end

#endif  // IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_TESTING_H_
