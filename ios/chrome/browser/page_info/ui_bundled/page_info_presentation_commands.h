// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_PRESENTATION_COMMANDS_H_

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
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

// Method invoked when the user wants to send us a feedback report.
- (void)showSendFeedbackPageForSender:(UserFeedbackSender)sender;

// Method invoked to open the tracking protection settings page.
- (void)showTrackingProtectionSettingsPage;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_PRESENTATION_COMMANDS_H_
