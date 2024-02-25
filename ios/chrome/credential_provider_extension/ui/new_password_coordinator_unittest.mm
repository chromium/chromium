// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator+Testing.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"
#import "testing/platform_test.h"

class NewPasswordCoordinatorTest : public PlatformTest {
 public:
  NewPasswordCoordinator* CreateCoordinatorWithServiceIdentifiers(
      NSArray<ASCredentialServiceIdentifier*>* identifiers) {
    UINavigationController* view_controller =
        [[UINavigationController alloc] init];
    return [[NewPasswordCoordinator alloc]
        initWithBaseViewController:view_controller
                serviceIdentifiers:identifiers
               existingCredentials:nil
         credentialResponseHandler:nil];
  }
};

// Tests that an alert message should be created without any crashes when new
// password is saved with an identifier that is aligned with RFC 1808.
TEST_F(NewPasswordCoordinatorTest, UrlAlignedWithRFC1808) {
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"http://a.com/test"
                        type:ASCredentialServiceIdentifierTypeURL];

  // Create the coordinator with the identifier that is aligned with RFC 1808.
  NewPasswordCoordinator* coordinator =
      CreateCoordinatorWithServiceIdentifiers(@[ serviceIdentifier ]);

  // Start the coordinator.
  [coordinator start];

  // Emulate the behavior when a user presses the button to save a password.
  [(NewPasswordViewController*)coordinator.viewController
          .topViewController alertUserCredentialExists];

  // Clean up.
  [coordinator stop];
}

// Tests that an alert message should be created without any crashes when new
// password is saved with an identifier that is aligned with RFC 3986 but not
// aligned with RFC 1808.
TEST_F(NewPasswordCoordinatorTest, UrlAlignedWithRFC3986Not1808) {
  // Schemeless URLs are valid in RFC 3986 but not RFC 1808.
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@"a.com/test"
                        type:ASCredentialServiceIdentifierTypeURL];

  // Create the coordinator with the identifier that is aligned with RFC 3986.
  NewPasswordCoordinator* coordinator =
      CreateCoordinatorWithServiceIdentifiers(@[ serviceIdentifier ]);

  // Start the coordinator.
  [coordinator start];

  // Emulate the behavior when a user presses the button to save a password.
  [(NewPasswordViewController*)coordinator.viewController
          .topViewController alertUserCredentialExists];

  // Clean up.
  [coordinator stop];
}

// Tests that an alert message should be created without any crashes when new
// password is saved with an invalid identifier.
TEST_F(NewPasswordCoordinatorTest, InvalidUrl) {
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:@""
                        type:ASCredentialServiceIdentifierTypeURL];

  // Create the coordinator with an empty string.
  NewPasswordCoordinator* coordinator =
      CreateCoordinatorWithServiceIdentifiers(@[ serviceIdentifier ]);

  // Start the coordinator.
  [coordinator start];

  // Emulate the behavior when a user presses the button to save a password.
  [(NewPasswordViewController*)coordinator.viewController
          .topViewController alertUserCredentialExists];

  // Clean up.
  [coordinator stop];
}
