// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"
#include "ios/web/common/user_agent.h"
#include "url/gurl.h"

@interface ShareToData : NSObject

// Designated initializer.
- (id)initWithShareURL:(const GURL&)shareURL
            visibleURL:(const GURL&)visibleURL
                 title:(NSString*)title
       isOriginalTitle:(BOOL)isOriginalTitle
       isPagePrintable:(BOOL)isPagePrintable
      isPageSearchable:(BOOL)isPageSearchable
             userAgent:(web::UserAgentType)userAgent
    thumbnailGenerator:
        (ChromeActivityItemThumbnailGenerator*)thumbnailGenerator;

// The URL to be shared with share extensions. This URL is the canonical URL of
// the page.
@property(nonatomic, readonly) const GURL& shareURL;
// The visible URL of the page.
@property(nonatomic, readonly) const GURL& visibleURL;

// NSURL versions of 'shareURL' and 'passwordManagerURL'. Use only for passing
// to libraries that take NSURL.
@property(nonatomic, readonly) NSURL* shareNSURL;
@property(nonatomic, readonly) NSURL* passwordManagerNSURL;

// Title to be shared (not nil).
@property(nonatomic, readonly, copy) NSString* title;
// Whether the title was provided by the page (i.e., was not generated from
// the url).
@property(nonatomic, readonly, assign) BOOL isOriginalTitle;
// Whether the page is printable or not.
@property(nonatomic, readonly, assign) BOOL isPagePrintable;
// Whether FindInPage can be enabled for this page.
@property(nonatomic, readonly, assign) BOOL isPageSearchable;
@property(nonatomic, readonly, assign) web::UserAgentType userAgent;
@property(nonatomic, readonly)
    ChromeActivityItemThumbnailGenerator* thumbnailGenerator;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_TO_DATA_H_
