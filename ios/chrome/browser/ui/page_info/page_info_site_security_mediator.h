// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

@class PageInfoSiteSecurityDescription;

// Mediator for the page info site security, extracting the data to be displayed
// for the view controller.
@interface PageInfoSiteSecurityMediator : NSObject

// For now this object only have static method.
- (instancetype)init NS_UNAVAILABLE;

// Returns a configuration based on the given WebState.
+ (PageInfoSiteSecurityDescription*)configurationForWebState:
    (web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_MEDIATOR_H_
