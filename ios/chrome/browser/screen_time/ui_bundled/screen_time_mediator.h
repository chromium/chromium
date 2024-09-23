// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ScreenTimeConsumer;
class WebStateList;

// This mediator manages reporting the active WebState's visible URL to the
// ScreenTime system.
@interface ScreenTimeMediator : NSObject
@property(nonatomic, weak) id<ScreenTimeConsumer> consumer;

// This mediator reports information from `webStateList` to the ScreenTime
// system. Recording is disabled if `suppressUsageRecording` is YES.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              suppressUsageRecording:(BOOL)suppressUsageRecording
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_MEDIATOR_H_
