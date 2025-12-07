// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol ContentSuggestionsViewControllerAudience;
@class AppBundlePromoConfig;
enum class ContentSuggestionsModuleType;
class AppStoreBundleService;
class PrefService;

// Handles App Bundle promo module events.
@protocol AppBundlePromoMediatorDelegate

// Indicates to the receiver that the App Bundle promo module should be removed.
// The `completion` is called after the removal is finished.
- (void)removeAppBundlePromoModuleWithCompletion:(ProceduralBlock)completion;

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

@end

// Mediator for managing the state of the App Bundle Promo (Magic Stack) module.
@interface AppBundlePromoMediator : NSObject

// Used by the App Bundle promo module for the module config.
@property(nonatomic, strong) AppBundlePromoConfig* config;

// Delegate.
@property(nonatomic, weak) id<AppBundlePromoMediatorDelegate> delegate;

// Audience for presentation actions.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    presentationAudience;

- (instancetype)initWithAppStoreBundleService:
                    (AppStoreBundleService*)appStoreBundleService
                           profilePrefService:(PrefService*)profilePrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Called when the promo is selected by the user.
- (void)didSelectAppBundlePromo;

// Removes the module from the Magic Stack on the current homepage without
// disabling the underlying feature. This prevents the module from being shown
// on the current homepage but does not affect its functionality elsewhere.
// The `completion` is called after the removal is finished.
- (void)removeModuleWithCompletion:(ProceduralBlock)completion;

// Presents the Best of Google bundle page in an App Store modal window using
// `baseViewController`. Runs `completion` when the sheet is dismissed.
- (void)presentAppStoreBundlePage:(UIViewController*)baseViewController
                   withCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_
