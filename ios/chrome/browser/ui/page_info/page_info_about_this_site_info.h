// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_INFO_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_INFO_H_

#import <Foundation/Foundation.h>

#import "url/gurl.h"

// Information about a page.
@interface PageInfoAboutThisSiteInfo : NSObject

// The description of the page.
@property(nonatomic, copy) NSString* summary;

// A link with more information about the page.
@property(nonatomic, assign) GURL moreAboutURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_INFO_H_
