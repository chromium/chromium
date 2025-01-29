// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_

#import <UIKit/UIKit.h>

// A callback that can be used to open a URL.
using GoogleOneOpenURLBlock = void (^)(NSURL*);

@protocol SystemIdentity;

enum GoogleOneEntryPoint {
  kSettings,
  kSaveToDriveAlert,
  kSaveToPhotosAlert,
};

@protocol GoogleOneController <NSObject>

// Initialization properties.
// A callback that will be used to open URLs.
@property(nonatomic, strong) GoogleOneOpenURLBlock openURLCallback;

// The identity for which Google One settings will be displayed.
@property(nonatomic, strong) id<SystemIdentity> identity;

// The entry point that triggered the controller.
@property(nonatomic, assign) GoogleOneEntryPoint entryPoint;

// Finalize the initialization of the controller.
// ALL the properties aboe must be set before calling this.
- (void)finalizeInitialization;

// Display the Google one settings on `viewController`.
- (void)launchWithViewController:(UIViewController*)viewController
                      completion:(void (^)(NSError*))completion;

@end

namespace ios {
namespace provider {

// Creates a one time GoogleOneController. Can return nil if Google One settings
// is not supported.
id<GoogleOneController> CreateGoogleOneController();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GOOGLE_ONE_GOOGLE_ONE_API_H_
