// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol LogoVendor;

// Handles updates to the Content Suggestions header.
@protocol ContentSuggestionsHeaderConsumer <NSObject>

// Whether the Google logo or doodle is being shown.
- (void)setLogoIsShowing:(BOOL)logoIsShowing;

// Exposes view and methods to drive the doodle.
- (void)setLogoVendor:(id<LogoVendor>)logoVendor;

// Tell location bar has taken focus.
- (void)locationBarBecomesFirstResponder;

// Sets whether voice search is currently enabled.
- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONSUMER_H_
