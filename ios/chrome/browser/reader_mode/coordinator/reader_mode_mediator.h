// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_consumer.h"

namespace web {
class WebState;
}

// Mediator for the Reader mode UI.
@interface ReaderModeMediator : NSObject

@property(nonatomic, weak) id<ReaderModeConsumer> consumer;

// Initializes the mediator using `webState` as the source WebState from which
// Reader mode content needs to be extracted.
- (instancetype)initWithWebState:(raw_ptr<web::WebState>)webState;

// Disconnects the mediator from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_MEDIATOR_H_
