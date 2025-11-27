// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <vector>

#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_consumer.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller_presentation_delegate.h"

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Protocol for the Mediator to request UI actions from the Coordinator.
@protocol CredentialExportMediatorDelegate <NSObject>

// Asks the delegate to fetch security domain secrets. This is only called if
// passkeys are detected in the export list.
- (void)fetchSecurityDomainSecretsWithCompletion:
    (void (^)(NSArray<NSData*>*))completion;

@end

// Mediator for the credential exchange export flow.
@interface CredentialExportMediator
    : NSObject <CredentialExportViewControllerPresentationDelegate>

// The consumer that receives updates about the credentials.
@property(nonatomic, weak) id<CredentialExportConsumer> consumer;

// Delegate of the mediator.
@property(nonatomic, weak) id<CredentialExportMediatorDelegate> delegate;

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
