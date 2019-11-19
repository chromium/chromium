// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/translate/translate_constants.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_table_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TranslateInfobarCoordinator () <InfobarCoordinatorImplementation,
                                           TranslateInfobarDelegateObserving,
                                           InfobarTranslateModalDelegate> {
  // Observer to listen for changes to the TranslateStep.
  std::unique_ptr<TranslateInfobarDelegateObserverBridge>
      _translateInfobarDelegateObserver;
}

// Delegate that holds the Translate Infobar information and actions.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* translateInfoBarDelegate;

// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;

// ModalViewController owned by this Coordinator.
@property(nonatomic, strong)
    InfobarTranslateTableViewController* modalViewController;

// The current state of translate.
@property(nonatomic, assign) translate::TranslateStep currentStep;

// Tracks user actions taken throughout Translate lifetime.
@property(nonatomic, assign) UserAction userAction;

// YES if translate is currently in progress
@property(nonatomic, assign) BOOL translateInProgress;

@end

@implementation TranslateInfobarCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize modalViewController = _modalViewController;

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  self = [super initWithInfoBarDelegate:infoBarDelegate
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeTranslate];
  if (self) {
    _translateInfoBarDelegate = infoBarDelegate;
    _translateInfobarDelegateObserver =
        std::make_unique<TranslateInfobarDelegateObserverBridge>(
            infoBarDelegate, self);
    _userAction = UserActionNone;
    _currentStep = translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
  }
  return self;
}

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  DCHECK(self.currentStep != step);
  self.currentStep = step;
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      self.translateInProgress = YES;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      [self.badgeDelegate infobarWasAccepted:self.infobarType
                                 forWebState:self.webState];
      ProceduralBlock completionBlock = ^{
        // Mark as not in progress after banner dismisses so that
        // InfobarCoordinator doesn't tell the ContainerViewController that
        // banner presentation is finished until after the "Page Translated"
        // banner is presented.
        self.translateInProgress = NO;
        [self createBannerViewController];
        // Present the "Show original" banner.
        [self presentInfobarBannerAnimated:YES completion:nil];
      };
      [self dismissInfobarBannerAnimated:YES completion:completionBlock];
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      break;
  }
  [self updateBannerTextForCurrentTranslateStep];
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  return self.userAction == UserActionNone;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    [self createBannerViewController];
  }
}

- (void)stop {
  [super stop];
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    self.delegate->RemoveInfoBar();
    [self.infobarContainer childCoordinatorStopped:self];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)isInfobarAccepted {
  return self.currentStep ==
         translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;
}

- (void)performInfobarAction {
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      self.userAction |= UserActionTranslate;

      // TODO(crbug.com/1014959): Add metrics
      if (self.translateInfoBarDelegate->ShouldAutoAlwaysTranslate()) {
        // TODO(crbug.com/1014959): Figure out if we should prompt user with
        // snackbar to auto always translate.
        self.translateInfoBarDelegate->ToggleAlwaysTranslate();
      }
      self.translateInfoBarDelegate->Translate();
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      self.userAction |= UserActionRevert;

      // TODO(crbug.com/1014959): Add metrics

      self.translateInfoBarDelegate->RevertWithoutClosingInfobar();
      // There is no completion signal (i.e. change of TranslateStep) in
      // translateInfoBarDelegate:didChangeTranslateStep:withErrorType: in
      // response to RevertWithoutClosingInfobar(), so revert Infobar badge
      // accepted state here.
      self.currentStep =
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
      [self.badgeDelegate infobarWasReverted:self.infobarType
                                 forWebState:self.webState];
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED() << "Translate infobar should not be able to perform its "
                      "action in this state.";
      break;
  }
}

- (void)infobarWasDismissed {
  self.bannerViewController = nil;
  self.modalViewController = nil;
}

#pragma mark - Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014959): implement
}

- (void)dismissBannerIfReady {
  if (!self.translateInProgress) {
    // Only attempt to dismiss banner if Translate is not in progress.
    [self.bannerViewController dismissWhenInteractionIsFinished];
  }
}

- (BOOL)infobarActionInProgress {
  return self.translateInProgress;
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  if (userInitiated && self.translateInfoBarDelegate)
    self.translateInfoBarDelegate->InfoBarDismissed();
}

#pragma mark - Modal

- (BOOL)configureModalViewController {
  // Return early if there's no delegate. e.g. A Modal presentation has been
  // triggered after the Infobar was destroyed, but before the badge/banner
  // were dismissed.
  if (!self.translateInfoBarDelegate)
    return NO;

  self.modalViewController =
      [[InfobarTranslateTableViewController alloc] initWithDelegate:self];
  self.modalViewController.title =
      l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_MODAL_TITLE);
  self.modalViewController.sourceLanguage = base::SysUTF16ToNSString(
      self.translateInfoBarDelegate->original_language_name());
  self.modalViewController.targetLanguage = base::SysUTF16ToNSString(
      self.translateInfoBarDelegate->target_language_name());
  self.modalViewController.shouldAlwaysTranslateSourceLanguage =
      self.translateInfoBarDelegate->ShouldAlwaysTranslate();
  self.modalViewController.translateButtonText = [self infobarButtonText];
  // TODO(crbug.com/1014959): Need to be able to toggle the modal button for
  // when translate is in progress.
  return YES;
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // TODO(crbug.com/1014959): implement
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  UITableView* tableView = self.modalViewController.tableView;
  // Update the tableView frame to then layout its content for |width|.
  tableView.frame = CGRectMake(0, 0, width, tableView.frame.size.height);
  [tableView setNeedsLayout];
  [tableView layoutIfNeeded];

  // Since the TableView is contained in a NavigationController get the
  // navigation bar height.
  CGFloat navigationBarHeight = self.modalViewController.navigationController
                                    .navigationBar.frame.size.height;

  return tableView.contentSize.height + navigationBarHeight;
}

#pragma mark - InfobarTranslateModalDelegate

- (void)alwaysTranslateSourceLanguage {
  // TODO(crbug.com/1014959):
}

- (void)neverTranslateSourceLanguage {
  // TODO(crbug.com/1014959):
}

- (void)neverTranslateSite {
  // TODO(crbug.com/1014959):
}

#pragma mark - Private

// Initialize and setup the banner.
- (void)createBannerViewController {
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:self.hasBadge
                  type:InfobarType::kInfobarTypeTranslate];
  [self updateBannerTextForCurrentTranslateStep];
  self.bannerViewController.iconImage =
      [UIImage imageNamed:@"infobar_translate_icon"];
  self.bannerViewController.optionalAccessibilityLabel =
      self.bannerViewController.titleText;
}

// Updates the banner's text for |self.currentStep|.
- (void)updateBannerTextForCurrentTranslateStep {
  self.bannerViewController.titleText = [self bannerTitleText];
  self.bannerViewController.buttonText = [self infobarButtonText];
  self.bannerViewController.subTitleText = [self bannerSubtitleText];
}

// Returns the title text of the banner depending on the |currentStep|.
- (NSString*)bannerTitleText {
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATING_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      NOTREACHED() << "Should not be presenting Banner in this TranslateStep";
      return nil;
  }
}

// Returns the subtitle text of the banner. Doesn't depend on state of
// |self.currentStep|.
- (NSString*)bannerSubtitleText {
  // Formatted as "[source] to [target]".
  return l10n_util::GetNSStringF(
      IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_BANNER_SUBTITLE,
      self.translateInfoBarDelegate->original_language_name(),
      self.translateInfoBarDelegate->target_language_name());
}

// Returns the text of the banner and modal action button depending on the
// |currentStep|.
- (NSString*)infobarButtonText {
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      return nil;
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      NOTREACHED() << "Translate infobar should not be presenting anything in "
                      "this state.";
      return nil;
  }
}

@end
