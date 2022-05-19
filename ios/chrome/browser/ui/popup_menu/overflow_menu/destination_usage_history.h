// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_H_

#import <UIKit/UIKit.h>

class PrefService;

// Maintains a history of which items from the new overflow menu carousel the
// user clicks. Additionally, suggests a sorted order (based on usage frecency)
// for carousel menu items.
@interface DestinationUsageHistory : NSObject

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

// Records a destination click from the overflow menu carousel.
- (void)trackDestinationClick:(NSString*)destinationName;

// Designated initializer. Initializes with |prefService|.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_H_
