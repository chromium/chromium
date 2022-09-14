// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_H_

#import <Foundation/Foundation.h>

@class MainContentUIState;

// Protocol for the UI displaying the main content area.
@protocol MainContentUI<NSObject>

// The current state of the main UI.
@property(nonatomic, readonly) MainContentUIState* mainContentUIState;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_H_
