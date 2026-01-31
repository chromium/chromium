// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class AppBundlePromoConfig;
@protocol AppBundlePromoMediatorDelegate;
class AppStoreBundleService;
enum class ContentSuggestionsModuleType;
@protocol ContentSuggestionsViewControllerAudience;
class PrefService;

// Mediator for managing the state of the App Bundle Promo (Magic Stack) module.
@interface AppBundlePromoMediator : NSObject

// Used by the App Bundle promo module for the module config.
@property(nonatomic, strong) AppBundlePromoConfig* config;

// Delegate for this mediator.
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

// Removes the module from the Magic Stack on the current homepage without
// disabling the underlying feature. This prevents the module from being shown
// on the current homepage but does not affect its functionality elsewhere.
// The `completion` is called after the removal is finished.
- (void)removeModuleWithCompletion:(ProceduralBlock)completion;

// Presents the Best of Google bundle page in an App Store modal window using
// `baseViewController`. Runs `completion` when the sheet is dismissed.
- (void)presentAppStoreBundlePage:(UIViewController*)baseViewController
                   withCompletion:(ProceduralBlock)completion;

// Called when the promo is selected by the user.
- (void)didSelectAppBundlePromo;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_H_
