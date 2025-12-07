// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_

#import <UIKit/UIKit.h>

@class NewTabPageColorPalette;
enum class SearchEngineLogoState;
@class SearchEngineLogoMediator;

// Handles updates to the NTP header.
@protocol NewTabPageHeaderConsumer <NSObject>

// Whether the Google logo or doodle is being shown.
- (void)setSearchEngineLogoState:(SearchEngineLogoState)logoState;

// Exposes view and methods to drive the doodle.
// TODO(crbug.com/436228514): The mediator should not be passed to the
// consumer.
- (void)setSearchEngineLogoMediator:
    (SearchEngineLogoMediator*)searchEngineLogoMediator;

// Sets whether voice search is currently enabled.
- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;

// Update account particle disc error badge.
- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email;

// Sets the default search engine name for display.
- (void)setDefaultSearchEngineName:(NSString*)dseName;

// Sets the default search engine icon for display.
- (void)setDefaultSearchEngineImage:(UIImage*)image;

// Whether AIM is allowed.
- (void)setAIMAllowed:(BOOL)allowed;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_CONSUMER_H_
