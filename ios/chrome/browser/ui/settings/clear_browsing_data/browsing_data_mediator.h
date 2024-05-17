// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_mutator.h"

class PrefService;

@protocol BrowsingDataConsumer;

// Mediator for Browsing Data. Used by the Quick Delete UI.
@interface BrowsingDataMediator : NSObject <BrowsingDataMutator>

@property(nonatomic, weak) id<BrowsingDataConsumer> consumer;

- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_BROWSING_DATA_MEDIATOR_H_
