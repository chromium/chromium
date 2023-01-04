// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#include "ios/web/common/user_agent.h"
#include "url/gurl.h"

@interface ShareToData : NSObject

// Designated initializer.
- (id)initWithShareURL:(const GURL&)shareURL
            visibleURL:(const GURL&)visibleURL
                 title:(NSString*)title
        additionalText:(NSString*)additionalText
       isOriginalTitle:(BOOL)isOriginalTitle
       isPagePrintable:(BOOL)isPagePrintable
      isPageSearchable:(BOOL)isPageSearchable
      canSendTabToSelf:(BOOL)canSendTabToSelf
             userAgent:(web::UserAgentType)userAgent
    thumbnailGenerator:(ChromeActivityItemThumbnailGenerator*)thumbnailGenerator
          linkMetadata:(LPLinkMetadata*)linkMetadata;

// The URL to be shared with share extensions. This URL is the canonical URL of
// the page.
@property(nonatomic, readonly) const GURL& shareURL;
// The visible URL of the page.
@property(nonatomic, readonly) const GURL& visibleURL;

// NSURL version of 'shareURL'. Use only for passing
// to libraries that take NSURL.
@property(nonatomic, readonly) NSURL* shareNSURL;

// Title to be shared (not nil).
@property(nonatomic, readonly, copy) NSString* title;

// Additional text to be shared, such as highlighted text. May be nil.
@property(nonatomic, readonly, copy) NSString* additionalText;

// Whether the title was provided by the page (i.e., was not generated from
// the url).
@property(nonatomic, readonly, assign) BOOL isOriginalTitle;
// Whether the page is printable or not.
@property(nonatomic, assign) BOOL isPagePrintable;
// Whether FindInPage can be enabled for this page.
@property(nonatomic, readonly, assign) BOOL isPageSearchable;
// Whether the current tab can be sent via Send-Tab-To-Self.
@property(nonatomic, readonly, assign) BOOL canSendTabToSelf;
@property(nonatomic, readonly, assign) web::UserAgentType userAgent;
@property(nonatomic, readonly)
    ChromeActivityItemThumbnailGenerator* thumbnailGenerator;
@property(nonatomic, readonly) LPLinkMetadata* linkMetadata;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_TO_DATA_H_
