// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_SHORTCUTS_HANDLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_SHORTCUTS_HANDLER_H_

#import <UIKit/UIKit.h>

// Instances conforming to this protocol handle the different shortcut actions
// in the new tab page.
@protocol NewTabPageShortcutsHandler <NSObject>

// Opens Lens View Finder.
- (void)openLensViewFinder;

// Opens the MIA experience.
- (void)openMIA;

// Preload views and view controllers needed for voice search.
- (void)preloadVoiceSearch;

// Initiates a voice search from a given view.
// Assumes `preloadVoiceSearch` was previously called by the embedder.
- (void)loadVoiceSearchFromView:(UIView*)voiceSearchSourceView;

// Opens a new incognito search.
- (void)openIncognitoSearch;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_SHORTCUTS_HANDLER_H_
