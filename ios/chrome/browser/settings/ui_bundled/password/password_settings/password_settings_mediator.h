// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_bulk_move_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_export_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_delegate.h"

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

class PrefService;
class TrustedVaultClientBackend;

struct CredentialCounts {
  int passwordCounts;
  int passkeyCounts;
};

@protocol ReauthenticationProtocol;
@protocol SystemIdentity;

// Mediator for the Password Settings screen.
@interface PasswordSettingsMediator : NSObject <PasswordSettingsDelegate>

@property(nonatomic, weak) id<PasswordSettingsConsumer> consumer;

// Creates a PasswordSettingsMediator. `reauthenticationModule` is used to gate
// access to the password export flow. `savedPasswordsPresenter` is used to
// check whether or not the user has saved passwords, and to get the password
// contents when the PasswordExporter is serializing them for export.
// `exportHandler` forwards certain events from the PasswordExporter so that
// alerts can be displayed.
- (instancetype)
       initWithReauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
              savedPasswordsPresenter:
                  (password_manager::SavedPasswordsPresenter*)passwordPresenter
    bulkMovePasswordsToAccountHandler:
        (id<BulkMoveLocalPasswordsToAccountHandler>)
            bulkMovePasswordsToAccountHandler
                        exportHandler:(id<PasswordExportHandler>)exportHandler
                          prefService:(PrefService*)prefService
                      identityManager:(signin::IdentityManager*)identityManager
                          syncService:(syncer::SyncService*)syncService
            trustedVaultClientBackend:
                (TrustedVaultClientBackend*)trustedVaultClientBackend
                             identity:(id<SystemIdentity>)identity
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Move the user's local passwords to the account store.
- (void)userDidStartBulkMoveLocalPasswordsToAccountFlow;

// Indicates that the user triggered the deletion flow.
- (void)userDidStartDeleteFlow;

// Indicates that the user triggered the export flow. `window` is used as an
// anchor for the OS views in the Credential Exchange feature flow.
- (void)userDidStartExportFlow:(UIWindow*)window;

// Indicates that the user completed the export flow.
- (void)userDidCompleteExportFlow;

// Indicates that the export flow was canceled while it was processing.
// The export flow can be canceled by the user or when reauthentication is
// required due to the app going to the background.
- (void)exportFlowCanceled;

// Detaches observers.
- (void)disconnect;

// Get the numbers of saved passwords and passkeys.
- (CredentialCounts)passwordAndPasskeyCounts;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_MEDIATOR_H_
