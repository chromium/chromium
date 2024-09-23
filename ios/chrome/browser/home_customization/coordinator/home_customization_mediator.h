// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"

@protocol HomeCustomizationDiscoverConsumer;
@protocol HomeCustomizationMagicStackConsumer;
@protocol HomeCustomizationMainConsumer;
@protocol HomeCustomizationNavigationDelegate;
class PrefService;

// The mediator for the Home surface's customization menu.
@interface HomeCustomizationMediator : NSObject <HomeCustomizationMutator>

// Initializes this mediator with a pref service.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// A consumer representing the main page's view controller.
@property(nonatomic, weak) id<HomeCustomizationMainConsumer> mainPageConsumer;

// A consumer representing the Discover page's view controller.
@property(nonatomic, weak) id<HomeCustomizationDiscoverConsumer>
    discoverPageConsumer;

// A consumer representing the Magic Stack page's view controller.
@property(nonatomic, weak) id<HomeCustomizationMagicStackConsumer>
    magicStackPageConsumer;

// The delegate which handles navigations within the menu.
@property(nonatomic, weak) id<HomeCustomizationNavigationDelegate>
    navigationDelegate;

// Sets the data for the main page's cells and sends it to the `consumer`.
- (void)configureMainPageData;

// Sets the data for the Discover page's cells and sends it to the `consumer`.
- (void)configureDiscoverPageData;

// Sets the data for the Magic Stack page's cells and sends it to the
// `consumer`.
- (void)configureMagicStackPageData;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_MEDIATOR_H_
