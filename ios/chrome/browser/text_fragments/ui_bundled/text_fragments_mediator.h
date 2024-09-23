// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_FRAGMENTS_UI_BUNDLED_TEXT_FRAGMENTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TEXT_FRAGMENTS_UI_BUNDLED_TEXT_FRAGMENTS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/text_fragments/text_fragments_manager.h"

// This class acts as a bridge between web and browser layers for Text Fragment
// features. It receives events delegated by the web layer and pipes them
// through to Chrome-specific behaviors.
@interface TextFragmentsMediator : NSObject <TextFragmentsDelegate>

// Initializes a new TextFragmentsMediator which will forward messages received
// in the web layer to a consumer, so the consumer can trigger UI changes in
// response.
- (instancetype)initWithConsumer:(id<TextFragmentsDelegate>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Attaches this mediator as the recipient for delegated events.
- (void)registerWithWebState:(web::WebState*)webState;

// Indicates to the web layer that JavaScript should be invoked to remove all
// text fragments (i.e., highlights) from the given WebState.
- (void)removeTextFragmentsInWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_TEXT_FRAGMENTS_UI_BUNDLED_TEXT_FRAGMENTS_MEDIATOR_H_
