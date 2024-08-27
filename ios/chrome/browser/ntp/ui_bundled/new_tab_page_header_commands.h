// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_COMMANDS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_COMMANDS_H_

// Commands protocol allowing the NewTabPageHeaderViewController to
// interact with the coordinator layer.
@protocol NewTabPageHeaderCommands

// Informs the receiver that the NewTabPageHeaderViewController's size
// has changed.
- (void)updateForHeaderSizeChange;

// Informs the receiver that the fakebox was tapped.
- (void)fakeboxTapped;

// Informs the receiver that the identity disc was tapped.
- (void)identityDiscWasTapped:(UIView*)identityDisc;

// Informs the receiver that the customization menu entrypoint was tapped.
- (void)customizationMenuWasTapped:(UIView*)customizationMenu;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_COMMANDS_H_
