// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"

#import "base/containers/cxx20_erase_vector.h"
#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/ui/settings/password/password_exporter.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSettingsMediator () <PasswordExporterDelegate,
                                        SavedPasswordsPresenterObserver> {
  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordManagerViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Service which gives us a view on users' saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;
}

// Helper object which maintains state about the "Export Passwords..." flow, and
// handles the actual serialization of the passwords.
@property(nonatomic, strong) PasswordExporter* passwordExporter;

// Delegate capable of showing alerts needed in the password export flow.
@property(nonatomic, weak) id<PasswordExportHandler> exportHandler;

// Whether or not there are any passwords saved.
@property(nonatomic, readwrite) BOOL hasSavedPasswords;

// Whether or not the password exporter is ready to be activated.
@property(nonatomic, readwrite) BOOL exporterIsReady;

@end

@implementation PasswordSettingsMediator

- (instancetype)
    initWithReauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
           savedPasswordsPresenter:
               (raw_ptr<password_manager::SavedPasswordsPresenter>)
                   passwordPresenter
                     exportHandler:(id<PasswordExportHandler>)exportHandler {
  self = [super init];
  if (self) {
    _passwordExporter =
        [[PasswordExporter alloc] initWithReauthenticationModule:reauthModule
                                                        delegate:self];
    _savedPasswordsPresenter = passwordPresenter;
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();
    _exportHandler = exportHandler;
  }
  return self;
}

- (void)setConsumer:(id<PasswordSettingsConsumer>)consumer {
  _consumer = consumer;
  // Now that the consumer is set, ensure that the consumer starts out with the
  // correct initial value for `canExportPasswords` or else the export button
  // will not behave correctly on load.
  self.exporterIsReady = self.passwordExporter.exportState == ExportState::IDLE;
  [self savedPasswordsDidChange:_savedPasswordsPresenter->GetSavedPasswords()];

  // TODO(crbug.com/1335156): Replace placeholder data with actual data piped
  // from observing pref and enterprise policy.
  [self.consumer setSavePasswordsEnabled:YES];
  [self.consumer setManagedByPolicy:NO];
}

- (void)userDidStartExportFlow {
  // Use GetSavedCredentials, rather than GetSavedPasswords, because the latter
  // can return duplicate passwords that shouldn't be included in the export.
  // However, this method also returns blocked sites ("Never save for
  // example.com"), so those must be filtered before passing to the exporter.
  std::vector<password_manager::CredentialUIEntry> passwords =
      _savedPasswordsPresenter->GetSavedCredentials();
  base::EraseIf(passwords, [](const auto& credential) {
    return credential.blocked_by_user;
  });
  [self.passwordExporter startExportFlow:passwords];
}

- (void)userDidCompleteExportFlow {
  [self.passwordExporter resetExportState];
}

- (void)userDidCancelExportFlow {
  [self.passwordExporter cancelExport];
}

#pragma mark - PasswordExporterDelegate

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:
                            (void (^)(NSString*, BOOL, NSArray*, NSError*))
                                completionHandler {
  [self.exportHandler showActivityViewWithActivityItems:activityItems
                                      completionHandler:completionHandler];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)errorReason {
  [self.exportHandler showExportErrorAlertWithLocalizedReason:errorReason];
}

- (void)showPreparingPasswordsAlert {
  [self.exportHandler showPreparingPasswordsAlert];
}

- (void)showSetPasscodeDialog {
  [self.exportHandler showSetPasscodeDialog];
}

- (void)updateExportPasswordsButton {
  // This is invoked by the exporter when its state changes, so we have to
  // re-read that state before pushing to the consumer.
  self.exporterIsReady = self.passwordExporter.exportState == ExportState::IDLE;
  [self pushExportStateToConsumerAndUpdate];
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange:
    (password_manager::SavedPasswordsPresenter::SavedPasswordsView)passwords {
  self.hasSavedPasswords = !passwords.empty();
  [self pushExportStateToConsumerAndUpdate];
}

#pragma mark - Private

// Pushes the current state of the exporter to the consumer and updates its
// export passwords button.
- (void)pushExportStateToConsumerAndUpdate {
  [self.consumer
      setCanExportPasswords:self.hasSavedPasswords && self.exporterIsReady];
  [self.consumer updateExportPasswordsButton];
}

@end
