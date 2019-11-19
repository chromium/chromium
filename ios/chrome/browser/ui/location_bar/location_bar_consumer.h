// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_

// Consumer for the location bar mediator.
@protocol LocationBarConsumer

// Notifies the consumer to update the location text.
// |clipTail| indicates whether the tail or the head should be clipped when the
// location text is too long.
- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail;
// Notifies the consumer to update the location icon and security status text.
- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText;

// Notifies the consumer about shareability of the current web page. Some web
// pages are not considered shareable (e.g. chrome://flags), and the share
// button for such pages should not be enabled.
- (void)updateLocationShareable:(BOOL)shareable;

// Notifies consumer to defocus the omnibox (for example on tab change).
- (void)defocusOmnibox;

// Notifies the consumer to update after a navigation to NTP. Will be called
// after -updateLocationText. Used for triggering NTP-specific location bar UI.
- (void)updateAfterNavigatingToNTP;

// Notifies the consumer to update after the search-by-image support status
// changes. (This is usually when the default search engine changes).
- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported;


@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_CONSUMER_H_
