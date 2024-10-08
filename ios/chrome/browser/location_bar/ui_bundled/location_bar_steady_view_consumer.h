// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the location bar steady view mediator.
@protocol LocationBarSteadyViewConsumer <NSObject>

// Notifies the consumer to update the location text.
// `clipTail` indicates whether the tail or the head should be clipped when the
// location text is too long.
- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail;

// Notifies the consumer to update the location icon and security status text.
- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText;

// Notifies the consumer about shareability of the current web page. Some web
// pages are not considered shareable (e.g. chrome://flags), and the share
// button for such pages should not be enabled.
- (void)updateLocationShareable:(BOOL)shareable;

// Notifies the consumer to update after a navigation to NTP. Will be called
// after -updateLocationText. Used for triggering NTP-specific location bar
// steady view UI.
- (void)updateAfterNavigatingToNTP;

// Attempts to show the lens overlay IPH.
- (void)attemptShowingLensOverlayIPH;

// Notifies the consumer to record the lens overlay entrypoint availability.
- (void)recordLensOverlayAvailability;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_
