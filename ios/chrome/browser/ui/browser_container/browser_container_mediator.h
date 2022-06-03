// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayPresenter;
class WebStateList;
@protocol BrowserContainerConsumer;

// Mediator that updates a BrowserContainerConsumer
@interface BrowserContainerMediator : NSObject

// Initializer for a mediator.  |webStateList| is the WebStateList for the
// Browser whose content is shown within the BrowserContainerConsumer.
// |overlayPresenter| is the OverlayModality::kWebContentArea OverlayPresenter.
// Both |webSateList| and |overlaPresenter| must be non-null.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
      webContentAreaOverlayPresenter:(OverlayPresenter*)overlayPresenter
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer.  Setting to a new value configures the new consumer.
@property(nonatomic, strong) id<BrowserContainerConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_MEDIATOR_H_
