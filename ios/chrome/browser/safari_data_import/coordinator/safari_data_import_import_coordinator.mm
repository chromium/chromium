// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_coordinator_transitioning_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface SafariDataImportImportCoordinator () <
    PromoStyleViewControllerDelegate>
@end

@implementation SafariDataImportImportCoordinator {
  /// The view controller pushed onto the base navigation controller; user
  /// interacts with it to present other views.
  SafariDataImportImportViewController* _containerViewController;
  /// The mediator handling the interaction with the model.
  SafariDataImportImportMediator* _mediator;
  /// File picker for the user to select Safari data.
  UIDocumentPickerViewController* _documentProvider;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _containerViewController =
      [[SafariDataImportImportViewController alloc] init];
  _containerViewController.delegate = self;
  _mediator = [[SafariDataImportImportMediator alloc] init];
  _mediator.importStageConsumer = _containerViewController;
  self.baseNavigationController.navigationBarHidden = NO;
  [self.baseNavigationController pushViewController:_containerViewController
                                           animated:YES];
}

- (void)stop {
  self.transitioningDelegate = nil;
  _containerViewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  switch (_containerViewController.importStage) {
    case SafariDataImportStage::kNotStarted:
      [_containerViewController
          transitionToImportStage:SafariDataImportStage::kFileLoading];
      [self showFilePicker];
      break;
    case SafariDataImportStage::kFileLoading:
      NOTREACHED() << "button should be disabled";
    case SafariDataImportStage::kReadyForImport:
    case SafariDataImportStage::kImporting:
    case SafariDataImportStage::kImported:
    default:
      break;
  }
}

- (void)didTapDismissButton {
  [self.transitioningDelegate
      safariDataImportCoordinatorWillDismissWorkflow:self];
}

#pragma mark - Private

/// Displays the document picker so the user could select the Safari data file.
- (void)showFilePicker {
  if (_documentProvider && (_documentProvider.isBeingPresented ||
                            _documentProvider.presentingViewController)) {
    return;
  }
  UTType* zip = [UTType typeWithIdentifier:@"com.pkware.zip-archive"];
  _documentProvider = [[UIDocumentPickerViewController alloc]
      initForOpeningContentTypes:@[ zip ]];
  _documentProvider.directoryURL =
      [[NSFileManager defaultManager] URLForDirectory:NSDownloadsDirectory
                                             inDomain:NSUserDomainMask
                                    appropriateForURL:nil
                                               create:NO
                                                error:nil];
  _documentProvider.allowsMultipleSelection = NO;
  _documentProvider.delegate = _mediator;
  _documentProvider.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  [_containerViewController presentViewController:_documentProvider
                                         animated:YES
                                       completion:nil];
}

@end
