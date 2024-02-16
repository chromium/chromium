// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_consumer.h"

namespace web {
class WebState;
}

namespace page_info {
class AboutThisSiteService;
}

@class PageInfoSiteSecurityDescription;

// Mediator for AboutThisPage that will extract the data to be displayed in the
// Page Info's view controller.
@interface PageInfoAboutThisSiteMediator : NSObject

// The consumer being set up by this mediator. Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<PageInfoAboutThisSiteConsumer> consumer;

// Designated initializer. `webState` is the webState for the BrowserContainer
// that owns this mediator. `service` is the AboutThisSiteService used to obtain
// the AboutThisSite information of the `webState`. `webState` should not be
// null.
- (instancetype)initWithWebState:(web::WebState*)webState
                         service:(page_info::AboutThisSiteService*)service;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_MEDIATOR_H_
