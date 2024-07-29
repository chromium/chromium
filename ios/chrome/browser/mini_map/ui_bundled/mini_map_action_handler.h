// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_ACTION_HANDLER_H_

// Handler for the action triggers in the MiniMapInterstitialViewController
@protocol MiniMapActionHandler

// User pressed accept
- (void)userPressedConsent;

// User pressed "No Thanks"
- (void)userPressedNoThanks;

// View was dismissed
- (void)dismissed;

// User pressed content settings
- (void)userPressedContentSettings;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_ACTION_HANDLER_H_
