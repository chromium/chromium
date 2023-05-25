// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol HistorySyncConsumer;

// Mediator that handles the sync operations.
@interface HistorySyncMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<HistorySyncConsumer> consumer;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
