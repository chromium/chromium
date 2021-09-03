// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include "url/gurl.h"

namespace web {
class WebState;
}

@protocol DiscoverFeedPreviewConsumer;

// The discover feed preview mediator that observes changes of the model and
// updates the corresponding consumer.
@interface DiscoverFeedPreviewMediator : NSObject

// The consumer that is updated by this mediator.
@property(nonatomic, weak) id<DiscoverFeedPreviewConsumer> consumer;

// Init the DiscoverFeedPreviewMediator with a |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
                      previewURL:(const GURL&)previewURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_MEDIATOR_H_
