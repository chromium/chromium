// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/credential_export_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/webauthn/coordinator/credential_export_mediator.h"
#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller.h"
#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller_presentation_delegate.h"

@interface CredentialExportCoordinator () <
    CredentialExportViewControllerPresentationDelegate>
@end

@implementation CredentialExportCoordinator {
  // Displays a view allowing the user to select credentials to export.
  CredentialExportViewController* _viewController;

  // Handles interaction with the credential export OS libraries.
  CredentialExportMediator* _mediator;

  // Used to fetch the user's saved passwords for export.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                         savedPasswordsPresenter:
                             (password_manager::SavedPasswordsPresenter*)
                                 savedPasswordsPresenter {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _savedPasswordsPresenter = savedPasswordsPresenter;
  }
  return self;
}

- (void)start {
  _viewController = [[CredentialExportViewController alloc] init];
  _viewController.delegate = self;

  _mediator = [[CredentialExportMediator alloc]
               initWithWindow:_baseNavigationController.view.window
      savedPasswordsPresenter:_savedPasswordsPresenter];

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

#pragma mark - CredentialExportViewControllerPresentationDelegate

- (void)userDidStartExport {
  if (@available(iOS 26, *)) {
    [_mediator startExport];
  }
}

@end
