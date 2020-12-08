// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_log.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/common/translate_constants.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/translate/translate_constants.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#import "ios/chrome/browser/translate/translate_infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_presentation_state.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_mediator.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_table_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kTranslateNotificationSnackbarCategory =
    @"TranslateNotificationSnackbarCategory";
}  // namespace

@interface TranslateInfobarCoordinator () <InfobarCoordinatorImplementation,
                                           TranslateInfobarDelegateObserving,
                                           InfobarTranslateModalDelegate> {
  // Observer to listen for changes to the TranslateStep.
  std::unique_ptr<TranslateInfobarDelegateObserverBridge>
      _translateInfobarDelegateObserver;
}

// The mediator managed by this Coordinator.
@property(nonatomic, strong) InfobarTranslateMediator* mediator;

// Delegate that holds the Translate Infobar information and actions.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* translateInfobarDelegate;

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

// YES if the Infobar has been accepted (translated the page).
@property(nonatomic, assign) BOOL infobarAccepted;

// YES if a "Show Original" banner can be presented.
@property(nonatomic, assign) BOOL displayShowOriginalBanner;
// Tracks the total number of translations in a page, including reverts to
// original.
@property(nonatomic, assign) NSUInteger translationsCount;

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
    _translateInfobarDelegate = infoBarDelegate;
    if (!base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
      _translateInfobarDelegateObserver =
          std::make_unique<TranslateInfobarDelegateObserverBridge>(
              infoBarDelegate, self);
    }
    _userAction = UserActionNone;
    _currentStep = translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
    // Legacy TranslateInfobarController logs this impression metric on init, so
    // log the impression here instead of in start() for consistency purposes.
    [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_IMPRESSION];
  }
  return self;
}

#pragma mark - TranslateInfobarDelegateObserving

// TODO(crbug.com/1025440): Move this to the mediator once it can push
// information to the banner.
- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  if (self.currentStep == step) {
    // No need to re-present or take any action if the new step is already the
    // same as the current state. (e.g. the page is already translated and
    // Translate is tapped in the overflow menu).
    return;
  }
  self.currentStep = step;
  self.mediator.currentStep = step;
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      self.translateInProgress = YES;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      self.displayShowOriginalBanner = YES;
      // Once the user asks for the page to be translated once, always make the
      // banner presentation high priority even if the user requests to show the
      // original language, since there is a possibility the user will be
      // toggling between languages. In addition, reverting an infobar does not
      // show the "Translate?" banner, so every subsequent banner presentation
      // will be a "Show Original" one.
      self.highPriorityPresentation = YES;
      [self.badgeDelegate infobarWasAccepted:self.infobarType
                                 forWebState:self.webState];

      // Log action for both manual and automatic translations.
      [self incrementAndRecordTranslationsCount];

      // If the Infobar hasn't been accepted but |step| changed to
      // TRANSLATE_STEP_AFTER_TRANSLATE it means that this was triggered by auto
      // translate.
      if (!self.infobarAccepted) {
        self.infobarAccepted = YES;
        if (!(self.infobarBannerState ==
              InfobarBannerPresentationState::NotPresented)) {
          [self dismissInfobarBannerAnimated:NO completion:nil];
        }
      }

      // If nothing is being presented present the "Show Original" banner, if
      // not it will be presented once the Banner or Modal is dismissed.
      if (!self.bannerViewController && !self.modalViewController) {
        [self presentShowOriginalBanner];
      }

      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      // Match TranslateInfobarController, which logs this for errrors.
      [self incrementAndRecordTranslationsCount];
      [self showErrorSnackbar];
      break;
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      break;
  }
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  return self.userAction == UserActionNone;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.mediator = [[InfobarTranslateMediator alloc]
        initWithInfoBarDelegate:self.translateInfobarDelegate];
    self.mediator.currentStep = self.currentStep;
    [self createBannerViewController];
  }
}

- (void)stop {
  [super stop];
  if (self.started) {
    self.started = NO;
    self.mediator = nil;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    if (self.delegate) {
      self.delegate->RemoveInfoBar();
    }
    if (self.userAction == UserActionNone) {
      [TranslateInfobarMetricsRecorder recordUnusedInfobar];
    }
    [self.infobarContainer childCoordinatorStopped:self];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)isInfobarAccepted {
  return self.infobarAccepted;
}

- (BOOL)infobarBannerActionWillPresentModal {
  return NO;
}

- (void)performInfobarAction {
  [self performInfobarActionForStep:self.currentStep];
}

- (void)infobarWasDismissed {
  self.bannerViewController = nil;
  self.modalViewController = nil;

  // After any Modal or Banner has been dismissed try to present the "Show
  // Original" banner.
  [self presentShowOriginalBanner];
}

#pragma mark - Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014959): implement
}

- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (BOOL)infobarActionInProgress {
  return self.translateInProgress;
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  if (userInitiated && self.translateInfobarDelegate)
    self.translateInfobarDelegate->InfoBarDismissed();
}

#pragma mark - Modal

- (BOOL)configureModalViewController {
  // Return early if there's no delegate. e.g. A Modal presentation has been
  // triggered after the Infobar was destroyed, but before the badge/banner
  // were dismissed.
  if (!self.translateInfobarDelegate)
    return NO;

  self.modalViewController =
      [[InfobarTranslateTableViewController alloc] initWithDelegate:self];
  self.modalViewController.title =
      l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_MODAL_TITLE);
  self.mediator.modalConsumer = self.modalViewController;
  MobileMessagesTranslateModalPresent modalPresent =
      self.currentStep ==
              translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE
          ? MobileMessagesTranslateModalPresent::
                PresentedAfterTranslatePromptBanner
          : MobileMessagesTranslateModalPresent::
                PresentedAfterTranslateFinishedBanner;
  [TranslateInfobarMetricsRecorder recordModalPresent:modalPresent];
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

- (void)showOriginalLanguage {
  DCHECK(self.currentStep ==
         translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE);
  [self performInfobarAction];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ShowOriginal];
  [self dismissInfobarModalAnimated:YES completion:nil];
}

- (void)translateWithNewLanguages {
  [self.mediator updateLanguagesIfNecessary];
  [self performInfobarActionForStep:translate::TranslateStep::
                                        TRANSLATE_STEP_BEFORE_TRANSLATE];
  [self dismissInfobarModalAnimated:YES completion:nil];
}

- (void)showChangeSourceLanguageOptions {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_PAGE_NOT_IN];
  [self recordLanguageDataHistogram:kLanguageHistogramPageNotInLanguage
                       languageCode:self.translateInfobarDelegate
                                        ->original_language_code()];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeSourceLanguage];
  InfobarTranslateLanguageSelectionTableViewController* languageSelectionTVC =
      [[InfobarTranslateLanguageSelectionTableViewController alloc]
                 initWithDelegate:self.mediator
          selectingSourceLanguage:YES];
  languageSelectionTVC.title = l10n_util::GetNSString(
      IDS_IOS_TRANSLATE_INFOBAR_SELECT_LANGUAGE_MODAL_TITLE);
  self.mediator.sourceLanguageSelectionConsumer = languageSelectionTVC;

  [self.modalViewController.navigationController
      pushViewController:languageSelectionTVC
                animated:YES];
}

- (void)showChangeTargetLanguageOptions {
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_MORE_LANGUAGES];
  [self recordLanguageDataHistogram:kLanguageHistogramMoreLanguages
                       languageCode:self.translateInfobarDelegate
                                        ->target_language_code()];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::ChangeTargetLanguage];
  InfobarTranslateLanguageSelectionTableViewController* languageSelectionTVC =
      [[InfobarTranslateLanguageSelectionTableViewController alloc]
                 initWithDelegate:self.mediator
          selectingSourceLanguage:NO];
  languageSelectionTVC.title = l10n_util::GetNSString(
      IDS_IOS_TRANSLATE_INFOBAR_SELECT_LANGUAGE_MODAL_TITLE);
  self.mediator.targetLanguageSelectionConsumer = languageSelectionTVC;

  [self.modalViewController.navigationController
      pushViewController:languageSelectionTVC
                animated:YES];
}

- (void)alwaysTranslateSourceLanguage {
  DCHECK(!self.translateInfobarDelegate->ShouldAlwaysTranslate());
  self.userAction |= UserActionAlwaysTranslate;
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_ALWAYS_TRANSLATE];
  [self recordLanguageDataHistogram:kLanguageHistogramAlwaysTranslate
                       languageCode:self.translateInfobarDelegate
                                        ->original_language_code()];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedAlwaysTranslate];
  self.translateInfobarDelegate->ToggleAlwaysTranslate();

  // Since toggle turned on always translate, translate now if not already
  // translated.
  if (self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE)
    [self performInfobarAction];

  [self dismissInfobarModalAnimated:YES completion:nil];
}

- (void)undoAlwaysTranslateSourceLanguage {
  DCHECK(self.translateInfobarDelegate->ShouldAlwaysTranslate());
  [self recordInfobarEvent:translate::InfobarEvent::
                               INFOBAR_ALWAYS_TRANSLATE_UNDO];
  self.translateInfobarDelegate->ToggleAlwaysTranslate();
  [self dismissInfobarModalAnimated:YES completion:nil];
}

- (void)neverTranslateSourceLanguage {
  DCHECK(self.translateInfobarDelegate->IsTranslatableLanguageByPrefs());
  self.userAction |= UserActionNeverTranslateLanguage;
  [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForSourceLanguage];
  [self recordLanguageDataHistogram:kLanguageHistogramNeverTranslate
                       languageCode:self.translateInfobarDelegate
                                        ->original_language_code()];
  self.translateInfobarDelegate->ToggleTranslatableLanguageByPrefs();
  [self dismissInfobarModalAnimated:YES
                         completion:^{
                           // Completely remove the Infobar along with its badge
                           // after adding site to never prompt list for the
                           // Website.
                           [self detachView];
                         }];
}

- (void)undoNeverTranslateSourceLanguage {
  DCHECK(!self.translateInfobarDelegate->IsTranslatableLanguageByPrefs());
  self.translateInfobarDelegate->ToggleTranslatableLanguageByPrefs();
  [self dismissInfobarModalAnimated:YES completion:nil];
  // TODO(crbug.com/1014959): implement else logic. Should anything be done?
}

- (void)neverTranslateSite {
  DCHECK(!self.translateInfobarDelegate->IsSiteOnNeverPromptList());
  self.userAction |= UserActionNeverTranslateSite;
  self.translateInfobarDelegate->ToggleNeverPrompt();
  [self
      recordInfobarEvent:translate::InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE];
  [TranslateInfobarMetricsRecorder
      recordModalEvent:MobileMessagesTranslateModalEvent::
                           TappedNeverForThisSite];
  [self dismissInfobarModalAnimated:YES
                         completion:^{
                           // Completely remove the Infobar along with its badge
                           // after adding site to never prompt list for the
                           // Website.
                           [self detachView];
                         }];
}

- (void)undoNeverTranslateSite {
  DCHECK(self.translateInfobarDelegate->IsSiteOnNeverPromptList());
  self.translateInfobarDelegate->ToggleNeverPrompt();
  [self dismissInfobarModalAnimated:YES completion:nil];
  // TODO(crbug.com/1014959): implement else logic. Should aything be done?
}

#pragma mark - Private

// Helper method for performInfobarAction, which also allows for
// translateWithNewLanguages() to execute a specific action without having to
// tempoarily modify self.currentStep.
- (void)performInfobarActionForStep:(translate::TranslateStep)step {
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE: {
      self.userAction |= UserActionTranslate;

      [self recordInfobarEvent:translate::InfobarEvent::
                                   INFOBAR_TARGET_TAB_TRANSLATE];
      [self recordLanguageDataHistogram:kLanguageHistogramTranslate
                           languageCode:self.translateInfobarDelegate
                                            ->target_language_code()];
      // TODO(crbug.com/1031184): Implement bannerActionWillBePerformed method
      // and log this there.
      if (self.baseViewController.presentedViewController &&
          self.baseViewController.presentedViewController ==
              self.bannerViewController) {
        [TranslateInfobarMetricsRecorder
            recordBannerEvent:MobileMessagesTranslateBannerEvent::Translate];
      }

      if (self.translateInfobarDelegate->ShouldAutoAlwaysTranslate()) {
        self.translateInfobarDelegate->ToggleAlwaysTranslate();
      }
      self.translateInfobarDelegate->Translate();
      self.infobarAccepted = YES;
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      self.userAction |= UserActionRevert;

      // Log for just reverts since translates are covered in
      // didChangeTranslateStep:.
      [self incrementAndRecordTranslationsCount];

      [self recordInfobarEvent:translate::InfobarEvent::INFOBAR_REVERT];
      // TODO(crbug.com/1031184): Implement bannerActionWillBePerformed method
      // and log this there.
      if (self.baseViewController.presentedViewController &&
          self.baseViewController.presentedViewController ==
              self.bannerViewController) {
        [TranslateInfobarMetricsRecorder
            recordBannerEvent:MobileMessagesTranslateBannerEvent::ShowOriginal];
      }

      self.translateInfobarDelegate->RevertWithoutClosingInfobar();
      self.infobarAccepted = NO;
      // There is no completion signal (i.e. change of TranslateStep) in
      // translateInfoBarDelegate:didChangeTranslateStep:withErrorType: in
      // response to RevertWithoutClosingInfobar(), so revert Infobar badge
      // accepted state here.
      self.currentStep =
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
      self.mediator.currentStep = self.currentStep;
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

// Presents the "Show Original" banner only if |self.displayShowOriginalBanner|
// is YES, meaning a translate event took place.
- (void)presentShowOriginalBanner {
  if (self.displayShowOriginalBanner) {
    self.displayShowOriginalBanner = NO;
    [self createBannerViewController];
    [self presentInfobarBannerAnimated:YES
                            completion:^{
                              self.translateInProgress = NO;
                            }];
  }
}

// Initialize and setup the banner.
- (void)createBannerViewController {
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:self.hasBadge
                  type:InfobarType::kInfobarTypeTranslate];
  [self updateBannerTextForCurrentTranslateStep];
  [self.bannerViewController
      setIconImage:[UIImage imageNamed:@"infobar_translate_icon"]];
  [self.bannerViewController
      setBannerAccessibilityLabel:[self bannerTitleText]];
}

// Updates the banner's text for |self.currentStep|.
- (void)updateBannerTextForCurrentTranslateStep {
  [self.bannerViewController setTitleText:[self bannerTitleText]];
  [self.bannerViewController setButtonText:[self infobarButtonText]];
  [self.bannerViewController setSubtitleText:[self bannerSubtitleText]];
}

// Returns the title text of the banner depending on the |currentStep|.
- (NSString*)bannerTitleText {
  switch (self.currentStep) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
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
      self.translateInfobarDelegate->original_language_name(),
      self.translateInfobarDelegate->target_language_name());
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
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      NOTREACHED() << "Translate infobar should not be presenting anything in "
                      "this state.";
      return nil;
  }
}

- (void)showErrorSnackbar {
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(IDS_TRANSLATE_NOTIFICATION_ERROR)];
  message.category = kTranslateNotificationSnackbarCategory;
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  id<SnackbarCommands> snackbarDispatcher =
      static_cast<id<SnackbarCommands>>(self.handler);
  [snackbarDispatcher showSnackbarMessage:message];
}

// Records a histogram for |event|.
- (void)recordInfobarEvent:(translate::InfobarEvent)event {
  UMA_HISTOGRAM_ENUMERATION(kEventHistogram, event);
}

// Records a histogram of |histogram| for |langCode|. This is used to log the
// language distribution of certain Translate events.
- (void)recordLanguageDataHistogram:(const std::string&)histogramName
                       languageCode:(const std::string&)langCode {
  // TODO(crbug.com/1025440): Use function version of macros here and in
  // TranslateInfobarController.
  base::SparseHistogram::FactoryGet(
      histogramName, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(metrics::MetricsLog::Hash(langCode));
}

// Increments the |kTranslationCountHistogram| histogram metric.
- (void)incrementAndRecordTranslationsCount {
  ++self.translationsCount;
  UMA_HISTOGRAM_COUNTS_1M(kTranslationCountHistogram, self.translationsCount);
}

@end
