// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_

#import "url/gurl.h"

@class PageInfoSiteSecurityDescription;

// Commands related to actions within the PageInfo UI.
@protocol PageInfoPresentationCommands

// Method invoked when the user requests more details about a page's security.
- (void)showSecurityPage;

// Method invoked when the user requests to see the security help page.
- (void)showSecurityHelpPage;

// Method invoked when the user requests more details about a page, i.e.
// taps on AboutThisSite.
- (void)showAboutThisSitePage:(GURL)URL;

// Method invoked in order to get the latest site security description.
- (PageInfoSiteSecurityDescription*)updatedSiteSecurityDescription;

// Method invoked when the user requests to see the Last Visited page.
- (void)showLastVisitedPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_
