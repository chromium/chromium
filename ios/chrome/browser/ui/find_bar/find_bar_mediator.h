// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/find_in_page/find_in_page_response_delegate.h"

@protocol FindBarConsumer;
@protocol FindInPageCommands;

namespace web {
class WebState;
}

// Mediator for the Find Bar and the Find In page feature. As this feature is
// currently being split off from BVC, this mediator will have more features
// added and is not an ideal example of the mediator pattern.
@interface FindBarMediator : NSObject <FindInPageResponseDelegate>

- (instancetype)initWithWebState:(web::WebState*)webState
                  commandHandler:(id<FindInPageCommands>)commandHandler;

@property(nonatomic, weak) id<FindBarConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_
