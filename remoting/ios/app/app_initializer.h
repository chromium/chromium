// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_APP_INITIALIZER_H_
#define REMOTING_IOS_APP_APP_INITIALIZER_H_

#import <UIKit/UIKit.h>

// This class is to allow different builds (Chromium vs internal) to do
// dependency injection before starting the app. Please see main.mm to see how
// it is used.
@interface AppInitializer : NSObject

// Called when the launch process has just begun.
+ (void)onAppWillFinishLaunching;

// Called when the launch process is almost done and the app's window is about
// to present.
+ (void)onAppDidFinishLaunching;

@end

#endif  // REMOTING_IOS_APP_APP_INITIALIZER_H_
