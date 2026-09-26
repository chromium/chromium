// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace base {
class FilePath;
}  // namespace base
class HomeBackgroundCustomizationService;
class PrefService;
@protocol HomeCustomizationEphemeralThemePromoConsumer;

// Mediator for the ephemeral theme promo UI.
@interface HomeCustomizationEphemeralThemePromoMediator : NSObject

// The consumer that receives updates from this mediator.
@property(nonatomic, weak) id<HomeCustomizationEphemeralThemePromoConsumer>
    consumer;

// Initializes the mediator with the profile `prefService`,
// `backgroundCustomizationService`, and `promoDataDirectory`.
- (instancetype)initWithPrefService:(PrefService*)prefService
     backgroundCustomizationService:
         (HomeBackgroundCustomizationService*)backgroundCustomizationService
                 promoDataDirectory:(const base::FilePath&)promoDataDirectory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Applies and stores the ephemeral theme as the current NTP background.
- (void)applyEphemeralTheme;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_MEDIATOR_H_
