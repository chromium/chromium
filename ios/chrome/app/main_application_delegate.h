// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_H_
#define IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_H_

#import <UIKit/UIKit.h>

@class AppState;

// The main delegate of the application.
@interface MainApplicationDelegate : UIResponder <UIApplicationDelegate>

// Handles the application stage changes.
@property(nonatomic, strong) AppState* appState;

@end

#endif  // IOS_CHROME_APP_MAIN_APPLICATION_DELEGATE_H_
