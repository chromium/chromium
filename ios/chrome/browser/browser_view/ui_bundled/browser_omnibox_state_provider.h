// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_OMNIBOX_STATE_PROVIDER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_OMNIBOX_STATE_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_state_provider.h"

/// A provider that aggregates omnibox state from multiple sources (e.g.
/// location bar, composebox).
@interface BrowserOmniboxStateProvider : NSObject <OmniboxStateProvider>

/// The provider for the location bar state.
@property(nonatomic, weak) id<OmniboxStateProvider> locationBarStateProvider;

/// The provider for the composebox state.
@property(nonatomic, weak) id<OmniboxStateProvider> composeboxStateProvider;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_OMNIBOX_STATE_PROVIDER_H_
