// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator_delegate.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_enterprise_sheet.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

}  // namespace

@interface YoutubeIncognitoCoordinator () <YoutubeIncognitoSheetDelegate,
                                           NewTabPageURLLoaderDelegate>
@end

@implementation YoutubeIncognitoCoordinator {
  YoutubeIncognitoSheet* _viewController;
  YoutubeIncognitoEnterpriseSheet* _entrepriseViewController;
}

- (void)start {
  if (self.incognitoDisabled) {
    [self presentEnterpriseViewController];
    return;
  }

  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (!localState->GetBoolean(prefs::kYoutubeIncognitoHasBeenShown) ||
      experimental_flags::AlwaysShowTheFirstPartyIncognitoUI()) {
    localState->SetBoolean(prefs::kYoutubeIncognitoHasBeenShown, true);
    [self presentViewController];
    return;
  }

  id<SnackbarCommands> snackbarHandler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  __weak __typeof(self) weakSelf = self;
  [snackbarHandler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_YOUTUBE_INCOGNITO_SNACKBAR_MESSAGE)
                   buttonText:
                       l10n_util::GetNSString(
                           IDS_IOS_YOUTUBE_INCOGNITO_SNACKBAR_BUTTON_TITLE)
                messageAction:^{
                  [weakSelf.tabOpener
                      dismissModalsAndMaybeOpenSelectedTabInMode:
                          ApplicationModeForTabOpening::NORMAL
                                               withUrlLoadParams:
                                                   UrlLoadParams::InNewTab(
                                                       GetLearnMoreIncognitoUrl())
                                                  dismissOmnibox:YES
                                                      completion:nil];
                }
             completionAction:nil];
}

- (void)stop {
  [super stop];
  [self dismissViewController];
  [self dismissEnterpriseViewController];
}

#pragma mark - YoutubeIncognitoSheetDelegate

- (void)didTapPrimaryActionButton {
  CHECK(_viewController || _entrepriseViewController);
  if (_viewController) {
    CHECK(!_entrepriseViewController);
    [self.delegate shouldStopYoutubeIncognitoCoordinator:self];
  } else if (_entrepriseViewController) {
    [self.tabOpener
        dismissModalsAndMaybeOpenSelectedTabInMode:
            ApplicationModeForTabOpening::NORMAL
                                 withUrlLoadParams:UrlLoadParams::InCurrentTab(
                                                       self.urlLoadParams
                                                           .web_params.url)
                                    dismissOmnibox:YES
                                        completion:nil];
  }
}

- (void)didTapSecondaryActionButton {
  CHECK(_entrepriseViewController && !_viewController);
  [self.delegate shouldStopYoutubeIncognitoCoordinator:self];
}

#pragma mark - NewTabPageURLLoaderDelegate

- (void)loadURLInTab:(const GURL&)URL {
  DCHECK(URL == GetLearnMoreIncognitoUrl());
  [self.tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                     NORMAL
                               withUrlLoadParams:UrlLoadParams::InNewTab(URL)
                                  dismissOmnibox:YES
                                      completion:nil];
}

#pragma mark - Private

// Dismisses the YoutubeIncognitoCoordinator's view controller.
- (void)dismissViewController {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

// Presents the YoutubeIncognitoCoordinator's view controller.
- (void)presentViewController {
  _viewController = [[YoutubeIncognitoSheet alloc] init];
  _viewController.delegate = self;
  _viewController.URLLoaderDelegate = self;
  _viewController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _viewController.sheetPresentationController.preferredCornerRadius =
      kHalfSheetCornerRadius;
  _viewController.sheetPresentationController
      .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)dismissEnterpriseViewController {
  [_entrepriseViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _entrepriseViewController = nil;
}

- (void)presentEnterpriseViewController {
  _entrepriseViewController = [[YoutubeIncognitoEnterpriseSheet alloc] init];
  _entrepriseViewController.delegate = self;
  _entrepriseViewController.URLText =
      base::SysUTF8ToNSString(self.urlLoadParams.web_params.url.spec());
  _entrepriseViewController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _entrepriseViewController.sheetPresentationController.preferredCornerRadius =
      kHalfSheetCornerRadius;
  _entrepriseViewController.sheetPresentationController
      .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  _entrepriseViewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  [self.baseViewController presentViewController:_entrepriseViewController
                                        animated:YES
                                      completion:nil];
}

@end
