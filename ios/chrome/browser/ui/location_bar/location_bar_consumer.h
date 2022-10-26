// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_

// Consumer for the location bar mediator.
@protocol LocationBarConsumer

// Notifies consumer to defocus the omnibox (for example on tab change).
- (void)defocusOmnibox;

// Notifies the consumer to update after the search-by-image support status
// changes. (This is usually when the default search engine changes).
- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported;

// Notifies the consumer to update after the Lens support status
// changes. (This is usually when the default search engine changes).
- (void)updateLensImageSupported:(BOOL)lensImageSupported;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_
