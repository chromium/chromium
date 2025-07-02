// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Represents the NTP parent view.
extern NSString* const kNTPViewIdentifier;

// Represents the NTP collection view.
extern NSString* const kNTPCollectionViewIdentifier;

// Represents the incognito NTP view.
extern NSString* const kNTPIncognitoViewIdentifier;

// Represents the feed header container.
extern NSString* const kNTPFeedHeaderIdentifier;

// Represents the sort button of the feed header.
extern NSString* const kNTPFeedHeaderSortButtonIdentifier;

// Represents the segmented control of the feed header.
extern NSString* const kNTPFeedHeaderSegmentedControlIdentifier;

// Represents the identity disc of the feed header.
extern NSString* const kNTPFeedHeaderIdentityDisc;

// Represents the customization menu button of the feed header.
extern NSString* const kNTPCustomizationMenuButtonIdentifier;

// Represents the identity disc badge.
extern NSString* const kNTPFeedHeaderIdentityDiscBadge;

// Represents the MIA entry point.
extern NSString* const kNTPMIAIdentifier;

// Represents the Lens quick action.
extern NSString* const kNTPLensQuickActionIdentifier;

// Represents the Voice Search quick action.
extern NSString* const kNTPVoiceSearchQuickActionIdentifier;

// Represents the incognito quick action.
extern NSString* const kNTPIncognitoQuickActionIdentifier;

// Represents the NTP omnibox.
extern NSString* const kNTPFakeOmniboxAccessibilityButton;

// The corner radius for the module containers on the Home surface.
extern const CGFloat kHomeModuleContainerCornerRadius;

// The minimum padding between the content and the edges of the screen,
// expressed as a percentage which includes both sides.
extern const CGFloat kHomeModuleMinimumPadding;

// The maximum number of times to show the new badge on the Customization menu
// entrypoint.
extern const NSInteger kCustomizationNewBadgeMaxImpressionCount;

// The bottom spacing underneath the Magic Stack.
extern const float kBottomMagicStackPadding;

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONSTANTS_H_
