// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_handler.h"

#import <set>

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view_delegate.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_logger.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The maximum number of tabs that can be selected in the Tab Picker.
constexpr NSUInteger kMaxTabAttachmentCount = 10;

}  // namespace

@interface GeminiTabPickerHandler () <SnackbarViewDelegate,
                                      TabPickerLogger,
                                      TabPickerSnackbarPresenter>
@end

@implementation GeminiTabPickerHandler {
  // The view controller that presented the Tab Picker modal.
  __weak UIViewController* _presentingViewController;
  // The active snackbar view presented on the tab picker window.
  SnackbarView* _snackbarView;
}

- (void)dealloc {
  [self dismissSnackbarAnimated:NO];
}

#pragma mark - GeminiTabPickerDelegate

- (void)openTabPickerFromViewController:
    (UIViewController*)presentingViewController {
  _presentingViewController = presentingViewController;

  TabPickerParams* params =
      [[TabPickerParams alloc] initWithSnackbarPresenter:self];
  params.baseViewController = presentingViewController;
  params.maxTabAttachmentCount = kMaxTabAttachmentCount;
  params.logger = self;
  if (self.selectedTabsProvider) {
    params.preselectedWebStateIDs = self.selectedTabsProvider();
  }
  params.PDFEnabled = IsPageContextPDFEnabled();

  __weak __typeof(self) weakSelf = self;
  TabPickerCompletionBlock completionBlock =
      ^(std::set<web::WebStateID> selectedIDs,
        std::set<web::WebStateID> cachedIDs) {
        if (weakSelf.selectionCallback) {
          weakSelf.selectionCallback(selectedIDs, cachedIDs);
        }
      };

  [self.tabPickerHandler showTabPickerWithParams:params
                                      completion:completionBlock];
}

#pragma mark - TabPickerLogger

- (void)logTabPickerShown {
  RecordGeminiTabPickerOpened();
}

- (void)logTabPickerHidden {
  RecordGeminiTabPickerDismissed();
}

#pragma mark - TabPickerSnackbarPresenter

- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit {
  [self
      showSnackbarWithTitle:l10n_util::GetPluralNSStringF(
                                IDS_IOS_GEMINI_TAB_PICKER_MAXIMUM_TABS_REACHED,
                                attachmentLimit)];
}

- (void)showCannotReloadTabError {
  [self showSnackbarWithTitle:
            l10n_util::GetNSString(
                IDS_IOS_GEMINI_TAB_PICKER_CANNOT_RELOAD_TAB_ERROR)];
}

- (void)showCannotAttachTabError {
  [self showSnackbarWithTitle:l10n_util::GetNSString(
                                  IDS_IOS_GEMINI_TAB_PICKER_CANT_BE_SHARED)];
}

#pragma mark - SnackbarViewDelegate

- (void)snackbarViewDidTapActionButton:(SnackbarView*)snackbarView {
  [self dismissSnackbarAnimated:YES];
}

- (void)snackbarViewDidRequestDismissal:(SnackbarView*)snackbarView {
  [self dismissSnackbarAnimated:YES];
}

#pragma mark - Private

// Displays a snackbar with the given `title` on the window presenting the Tab
// Picker.
// TODO(crbug.com/509898861): Once bottom sheet migration is complete, we can
// remove `_snackbarView` and use the Chrome Snackbar Handler instead.
- (void)showSnackbarWithTitle:(NSString*)title {
  [self dismissSnackbarAnimated:NO];

  UIWindow* window = _presentingViewController.view.window;
  if (!window) {
    return;
  }

  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  _snackbarView = [[SnackbarView alloc] initWithMessage:message];
  _snackbarView.delegate = self;
  _snackbarView.translatesAutoresizingMaskIntoConstraints = NO;

  [window addSubview:_snackbarView];
  AddSameConstraints(_snackbarView, window);
  [_snackbarView presentAnimated:YES completion:nil];
}

// Dismisses the active snackbar view and removes it from the window hierarchy.
// TODO(crbug.com/509898861): Once bottom sheet migration is complete, we can
// remove `_snackbarView` and use the Chrome Snackbar Handler instead.
- (void)dismissSnackbarAnimated:(BOOL)animated {
  if (!_snackbarView) {
    return;
  }
  SnackbarView* snackbarView = _snackbarView;
  _snackbarView.delegate = nil;
  _snackbarView = nil;

  [snackbarView dismissAnimated:animated
                     completion:^{
                       [snackbarView removeFromSuperview];
                     }];
}

@end
