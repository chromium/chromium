// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/google_one/shared/google_one_entry_point.h"

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
@property(nonatomic, strong) void (^flowDidEndWithErrorCallback)(NSError*);

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

namespace ios::provider {

id<GoogleOneController> CreateGoogleOneController(
    GoogleOneConfiguration* configuration);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
