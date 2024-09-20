// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol LogoVendor;

// Handles updates to the NTP header.
@protocol NewTabPageHeaderConsumer <NSObject>

// Whether the Google logo or doodle is being shown.
- (void)setLogoIsShowing:(BOOL)logoIsShowing;

// Exposes view and methods to drive the doodle.
- (void)setLogoVendor:(id<LogoVendor>)logoVendor;

// Sets whether voice search is currently enabled.
- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;

// Update account particle disc error badge.
- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
