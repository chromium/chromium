// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ACCESSIBILITY_IDENTIFIER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ACCESSIBILITY_IDENTIFIER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the Switch to Open Tab button.
extern NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier;

// Accessibility identifier for the Append button.
extern NSString* const kOmniboxPopupRowAppendAccessibilityIdentifier;

// A11y identifier for the table view containing suggestions.
extern NSString* const kOmniboxPopupTableViewAccessibilityIdentifier;

// Helper to generate omnibox popup accessibility identifiers.
@interface OmniboxPopupAccessibilityIdentifierHelper : NSObject

// Generate omnibox popup row accessibility identifier at `indexPath`.
+ (NSString*)accessibilityIdentifierForRowAtIndexPath:(NSIndexPath*)indexPath;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_ACCESSIBILITY_IDENTIFIER_CONSTANTS_H_
