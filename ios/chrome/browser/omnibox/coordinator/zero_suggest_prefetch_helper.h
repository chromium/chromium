// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_ZERO_SUGGEST_PREFETCH_HELPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_ZERO_SUGGEST_PREFETCH_HELPER_H_

#import <UIKit/UIKit.h>

class WebStateList;
@class OmniboxAutocompleteController;

/// This object starts ZPS prefetch in the `autocompleteController` whenever an
/// NTP is displayed in `webStateList`, specifically: upon creation of this
/// object if the active web state is showing NTP; whenever a webstate that
/// displays NTP is activated, or whenever the active web state navigates to the
/// NTP.
@interface ZeroSuggestPrefetchHelper : NSObject

// Observed web state list.
@property(nonatomic, readonly, assign) WebStateList* webStateList;
// The omnibox autocomplete controller for prefetching.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

// Designated initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;
- (instancetype)init NS_UNAVAILABLE;

/// Disconnects all observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_ZERO_SUGGEST_PREFETCH_HELPER_H_
