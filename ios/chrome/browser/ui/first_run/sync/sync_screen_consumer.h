// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles sync screen UI updates.
@protocol SyncScreenConsumer <NSObject>

// Sets the UI as interactable or not.
- (void)setUIEnabled:(BOOL)UIEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_CONSUMER_H_
