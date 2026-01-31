// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayPresenter;
class WebStateList;
@protocol BrowserContentConsumer;

// Mediator that updates a BrowserContentConsumer
@interface BrowserContentMediator : NSObject

// Initializer for a mediator.  `webStateList` is the WebStateList for the
// Browser whose content is shown within the BrowserContentConsumer.
// `overlayPresenter` is the OverlayModality::kWebContentArea OverlayPresenter.
// Both `webSateList` and `overlaPresenter` must be non-null.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
      webContentAreaOverlayPresenter:(OverlayPresenter*)overlayPresenter
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer.  Setting to a new value configures the new consumer.
@property(nonatomic, strong) id<BrowserContentConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_MEDIATOR_H_
