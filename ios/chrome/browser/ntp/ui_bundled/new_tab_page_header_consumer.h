// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_

#import <UIKit/UIKit.h>

@class NewTabPageColorPalette;

// Handles updates to the NTP header.
@protocol NewTabPageHeaderConsumer <NSObject>
// Sets whether voice search is currently enabled.
- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;

// Update account particle disc error badge.
// `name` may be nil.
- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email;

// Sets the default search engine name for display.
- (void)setDefaultSearchEngineName:(NSString*)dseName;

// Sets the default search engine icon for display.
- (void)setDefaultSearchEngineImage:(UIImage*)image;

// Whether AIM is allowed.
- (void)setAIMAllowed:(BOOL)allowed;

// Whether the current session is eligible for fusebox.
- (void)setFuseboxEligible:(BOOL)eligible;

// Whether the omnibox is pinned to the bottom position.
- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
