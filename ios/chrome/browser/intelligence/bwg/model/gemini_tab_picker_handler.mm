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
#import "ios/chrome/browser/tab_picker/public/tab_picker_logger.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The maximum number of tabs that can be selected in the Tab Picker.
constexpr NSUInteger kMaxTabAttachmentCount = 10;

}  // namespace

@interface GeminiTabPickerHandler () <TabPickerSnackbarPresenter,
                                      TabPickerLogger>
@end

@implementation GeminiTabPickerHandler

#pragma mark - GeminiTabPickerDelegate

- (void)openTabPickerFromViewController:
    (UIViewController*)presentingViewController {
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
  // TODO(crbug.com/522850674): Update once snackbar copy is finalized.
  [self showSnackbarWithTitle:l10n_util::GetPluralNSStringF(
                                  IDS_IOS_COMPOSEBOX_MAXIMUM_TABS_REACHED,
                                  attachmentLimit)];
}

- (void)showCannotReloadTabError {
  // TODO(crbug.com/522850674): Update once snackbar copy is finalized.
  [self showSnackbarWithTitle:l10n_util::GetNSString(
                                  IDS_IOS_COMPOSEBOX_CANNOT_RELOAD_TAB_ERROR)];
}

- (void)showCannotAttachTabError {
  // TODO(crbug.com/522850674): Update once snackbar copy is finalized.
  [self showSnackbarWithTitle:l10n_util::GetNSString(
                                  IDS_IOS_COMPOSEBOX_UNABLE_TO_ADD_ATTACHMENT)];
}

#pragma mark - Private

// Displays a snackbar with the given `title`. `bottomOffset` is set to 0 to
// override default toolbar padding, since the Tab Picker covers the toolbar.
- (void)showSnackbarWithTitle:(NSString*)title {
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  [self.snackbarHandler showSnackbarMessage:message bottomOffset:0];
}

@end
