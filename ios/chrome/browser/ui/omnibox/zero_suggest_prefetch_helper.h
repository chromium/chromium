// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_ZERO_SUGGEST_PREFETCH_HELPER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_ZERO_SUGGEST_PREFETCH_HELPER_H_

#import <UIKit/UIKit.h>

class WebStateList;
class OmniboxController;

/// This object starts ZPS prefetch in the `autocompleteController` whenever an
/// NTP is displayed in `webStateList`, specifically: upon creation of this
/// object if the active web state is showing NTP; whenever a webstate that
/// displays NTP is activated, or whenever the active web state navigates to the
/// NTP.
@interface ZeroSuggestPrefetchHelper : NSObject

// Observed web state list.
@property(nonatomic, readonly, assign) WebStateList* webStateList;
// The omnibox controller for prefetching.
@property(nonatomic, readonly, assign) OmniboxController* controller;

// Designated initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                          controller:(OmniboxController*)controller;
- (instancetype)init NS_UNAVAILABLE;

/// Disconnects all observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_ZERO_SUGGEST_PREFETCH_HELPER_H_
