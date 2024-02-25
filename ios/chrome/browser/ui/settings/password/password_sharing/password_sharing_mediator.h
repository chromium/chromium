// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/scoped_refptr.h"

@protocol PasswordSharingMediatorDelegate;
@class RecipientInfoForIOSDisplay;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace password_manager {
struct CredentialUIEntry;
class PasswordSenderService;
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace signin {
class IdentityManager;
}  // namespace signin

// This mediator fetches information about the family members of the user (their
// display info and eligibility for receiving shared passwords) and notifies the
// coordinator with the result. It also handles sending passwords to recipients
// at the end of the password sharing flow.
@interface PasswordSharingMediator : NSObject

- (instancetype)initWithDelegate:(id<PasswordSharingMediatorDelegate>)delegate
          sharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                 identityManager:(signin::IdentityManager*)identityManager
         savedPasswordsPresenter:
             (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
           passwordSenderService:
               (password_manager::PasswordSenderService*)passwordSenderService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Fetches corresponding password forms for `selectedCredential` and invokes
// SendPasswords method of PasswordSenderService with all forms for each of the
// `selectedRecipients`.
- (void)sendSelectedCredentialToSelectedRecipients;

// Credential selected by the user to be shared.
@property(nonatomic, assign)
    password_manager::CredentialUIEntry selectedCredential;

// Recipients selected by the user to receive the shared passwords.
@property(nonatomic, strong)
    NSArray<RecipientInfoForIOSDisplay*>* selectedRecipients;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_MEDIATOR_H_
