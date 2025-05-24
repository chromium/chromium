// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/google_one/shared/google_one_entry_point.h"

/** The possible outcomes of the Google One flow. */
enum class GoogleOneOutcome {
  // There was no error (and so the user purchased a plan).
  kGoogleOneEntryOutcomeNoError = 0,
  // There was an unknown error, probably not directly related to the purchase.
  kGoogleOneEntryOutcomeUnknownError = 1,
  // There was an unknown error, probably related to the purchase.
  kGoogleOneEntryOutcomeErrorUnspecified = 2,
  // The purchase flow was cancelled (by the user or the app).
  kGoogleOneEntryOutcomePurchaseCancelled = 3,
  // The purchase flow was already presented (and so could not be started
  // again).
  kGoogleOneEntryOutcomeAlreadyPresented = 4,
  // The made a purchase and it failed.
  kGoogleOneEntryOutcomePurchaseFailed = 5,
  // The flow will continue in another app.
  kGoogleOneEntryOutcomeWillLeaveApp = 6,
  // The flow failed to launch due to an error.
  kGoogleOneEntryOutcomeLaunchFailed = 7,
  // The flow failed to launch due to invalid parameters.
  kGoogleOneEntryOutcomeInvalidParameters = 8,
};

@protocol SystemIdentity;

// The configuration for the GoogleOneController.
@interface GoogleOneConfiguration : NSObject

// The entry point that triggered the controller.
@property(nonatomic, assign) GoogleOneEntryPoint entryPoint;

// The identity for which Google One settings will be displayed.
@property(nonatomic, strong) id<SystemIdentity> identity;

// A callback that will be used to open URLs.
@property(nonatomic, strong) void (^openURLCallback)(NSURL*);

// A callback that will is called at the end of the Google One flow.
@property(nonatomic, strong) void (^flowDidEndWithErrorCallback)
    (GoogleOneOutcome, NSError*);

@end

@protocol GoogleOneController <NSObject>

// Launch the GoogleOneController. This will present the Google One VC on top of
// `viewController`.
// `completion` is a flow completion.
- (void)launchWithViewController:(UIViewController*)viewController
                      completion:(void (^)(NSError*))completion;

// Stop the GoogleOneController.
// This will dismiss the viewController presented by `launchWithViewController`.
// Do not call if the flow completion was already called (the service is already
// stopped).
- (void)stop;

@end

// A protocol to replace the Google One providers in tests.
@protocol GoogleOneControllerFactory

// Create a GoogleOneController.
- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration;

@end

namespace ios::provider {

// Override the GoogleOne controller creator for tests.
void SetGoogleOneControllerFactory(id<GoogleOneControllerFactory> factory);

// Creates an instance of GoogleOneController.
id<GoogleOneController> CreateGoogleOneController(
    GoogleOneConfiguration* configuration);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
