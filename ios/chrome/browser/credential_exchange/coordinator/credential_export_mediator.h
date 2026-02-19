// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <vector>

#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_consumer.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_favicon_provider.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/passwords/coordinator/password_export_handler.h"

namespace password_manager {
class AffiliatedGroup;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

class FaviconLoader;
@protocol ReauthenticationProtocol;

// Protocol for the Mediator to request UI actions from the Coordinator.
@protocol CredentialExportMediatorDelegate <NSObject>

// Asks the delegate to fetch trusted vault keys. This is only called if
// passkeys are detected in the export list.
- (void)fetchTrustedVaultKeysWithCompletion:
    (void (^)(webauthn::SharedKeyList))completion;

// Asks the delegate to display a generic error alert.
- (void)showGenericError;

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
        reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
                 exportHandler:(id<PasswordExportHandler>)exportHandler
                   syncService:(syncer::SyncService*)syncService
               identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
