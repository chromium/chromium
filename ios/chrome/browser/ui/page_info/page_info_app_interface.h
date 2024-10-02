// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App Interface to interact with Page Info.
@interface PageInfoAppInterface : NSObject

// Adds an AboutThisSite hint to the OptimizationGuide of the original
// Profile.
+ (void)addAboutThisSiteHintForURL:(NSString*)url
                       description:(NSString*)description
                  aboutThisSiteURL:(NSString*)aboutThisSiteURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_APP_INTERFACE_H_
