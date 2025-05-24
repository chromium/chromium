// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@protocol ReaderModeConsumer;

// Mediator for the Reader mode UI.
@interface ReaderModeMediator : NSObject

@property(nonatomic, weak) id<ReaderModeConsumer> consumer;

// Initializes the mediator for the given `webStateList`.
- (instancetype)initWithWebStateList:(raw_ptr<WebStateList>)webStateList;

// Disconnects the mediator from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_
