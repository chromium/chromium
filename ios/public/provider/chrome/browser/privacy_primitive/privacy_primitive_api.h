// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_API_H_

#import <UIKit/UIKit.h>

@class PrivacyPrimitiveConfiguration;

/// Protocol representing a server-driven Privacy Primitive / ConsentKit
/// service.
@protocol PrivacyPrimitiveService <NSObject>

/// Displays the server-driven consent screen.
/// `completionHandler` is called with `YES` if the flow completed successfully,
/// or `NO` if cancelled or failed.
- (void)showFlowWithPresentingViewController:(UIViewController*)viewController
                           completionHandler:
                               (void (^)(BOOL success))completionHandler;

@end

namespace ios::provider {

/// Creates a new instance of `PrivacyPrimitiveService`.
/// Returns `nil` if privacy primitives are unsupported or configuration is
/// invalid.
id<PrivacyPrimitiveService> CreatePrivacyPrimitiveService(
    PrivacyPrimitiveConfiguration* configuration);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_API_H_
