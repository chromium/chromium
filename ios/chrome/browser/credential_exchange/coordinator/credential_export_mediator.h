// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/credential_exchange/ui/credential_export_consumer.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_favicon_provider.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller_presentation_delegate.h"

namespace password_manager {
class AffiliatedGroup;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

class FaviconLoader;

// Protocol for the Mediator to request UI actions from the Coordinator.
@protocol CredentialExportMediatorDelegate <NSObject>

// Asks the delegate to fetch trusted vault keys. This is only called if
// passkeys are detected in the export list.
- (void)fetchTrustedVaultKeysWithCompletion:
    (void (^)(NSArray<NSData*>*))completion;

@end

// Mediator for the credential exchange export flow.
@interface CredentialExportMediator
    : NSObject <CredentialExportViewControllerPresentationDelegate,
                CredentialExportFaviconProvider>

// The consumer that receives updates about the credentials.
@property(nonatomic, weak) id<CredentialExportConsumer> consumer;

// Delegate of the mediator.
@property(nonatomic, weak) id<CredentialExportMediatorDelegate> delegate;

- (instancetype)initWithWindow:(UIWindow*)window
              affiliatedGroups:(std::vector<password_manager::AffiliatedGroup>)
                                   affiliatedGroups
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel
                 faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
