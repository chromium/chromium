// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ToolbarConsumer;

namespace bookmarks {
class BookmarkModel;
}
namespace web {
class WebState;
}
class TemplateURLService;
class WebStateList;

// A mediator object that provides the relevant properties of a web state
// to a consumer.
@interface ToolbarMediator : NSObject

// TemplateURLService used to check the default search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// The WebStateList that this mediator listens for any changes on the total
// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// The bookmarks model to know if the page is bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<ToolbarConsumer> consumer;

// Updates the consumer to conforms to |webState|.
- (void)updateConsumerForWebState:(web::WebState*)webState;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
