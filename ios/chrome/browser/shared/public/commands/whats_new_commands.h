// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_

// Commands to control the display of user eduction promotional UI.
@protocol DefaultPromoCommands <NSObject>

// Display a tailored modal promotional UI about the iOS14 default browser
// feature.
- (void)showTailoredPromoStaySafe;

// Display a tailored modal promotional UI about the iOS14 default browser
// feature.
- (void)showTailoredPromoMadeForIOS;

// Display a tailored modal promotional UI about the iOS14 default browser
// feature.
- (void)showTailoredPromoAllTabs;

// Display a regular modal promotional UI about the iOS14 default browser
// feature.
- (void)showDefaultBrowserFullscreenPromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_
