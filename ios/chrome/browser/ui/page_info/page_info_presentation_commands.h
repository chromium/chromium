// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_

// Commands related to actions within the PageInfo UI.
@protocol PageInfoPresentationCommands

// Method invoked when the user requests more details about a page's security.
- (void)showSecurityPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PRESENTATION_COMMANDS_H_
