// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSUMER_H_

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_placeholder_type.h"

// Consumer for the location bar mediator.
@protocol LocationBarConsumer

// Notifies consumer to defocus the omnibox (for example on tab change).
- (void)defocusOmnibox;

// Notifies the consumer to update after the search-by-image support status
// changes. (This is usually when the default search engine changes).
- (void)setSearchByImageEnabled:(BOOL)searchByImageSupported;

// Notifies the consumer to update after the Lens support status
// changes. (This is usually when the default search engine changes).
- (void)setLensImageEnabled:(BOOL)lensImageSupported;

// Set the placeholder view type to be displayed in case there is no badge view
// nor contextual panel entrypoint.
- (void)setPlaceholderType:(LocationBarPlaceholderType)placeholderType;
@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSUMER_H_
