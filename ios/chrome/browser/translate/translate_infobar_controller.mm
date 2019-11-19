// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_controller.h"

#import <UIKit/UIKit.h>

#include <stddef.h>
#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_log.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#include "ios/chrome/browser/translate/language_selection_context.h"
#include "ios/chrome/browser/translate/language_selection_delegate.h"
#include "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/translate/translate_constants.h"
#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"
#include "ios/chrome/browser/translate/translate_option_selection_delegate.h"
#include "ios/chrome/browser/translate/translate_option_selection_handler.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/translate/translate_notification_delegate.h"
#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Whether langugage selection popup menu is offered; and whether it is to
// select the source or the target language.
typedef NS_ENUM(NSInteger, LanguageSelectionState) {
  LanguageSelectionStateNone,
  LanguageSelectionStateSource,
  LanguageSelectionStateTarget,
};

// UMA histogram names.
// Note: These string constants are repeated in TranslateCompactInfoBar.java.
const char kLanguageHistogramTranslate[] =
    "Translate.CompactInfobar.Language.Translate";
const char kLanguageHistogramMoreLanguages[] =
    "Translate.CompactInfobar.Language.MoreLanguages";
const char kLanguageHistogramPageNotInLanguage[] =
    "Translate.CompactInfobar.Language.PageNotIn";
const char kLanguageHistogramAlwaysTranslate[] =
    "Translate.CompactInfobar.Language.AlwaysTranslate";
const char kLanguageHistogramNeverTranslate[] =
    "Translate.CompactInfobar.Language.NeverTranslate";
const char kEventHistogram[] = "Translate.CompactInfobar.Event";
const char kTranslationCountHistogram[] =
    "Translate.CompactInfobar.TranslationsPerPage";

}  // namespace

@interface TranslateInfoBarController () <LanguageSelectionDelegate,
                                          TranslateInfobarDelegateObserving,
                                          TranslateInfobarViewDelegate,
                                          TranslateNotificationDelegate,
                                          TranslateOptionSelectionDelegate> {
  std::unique_ptr<TranslateInfobarDelegateObserverBridge>
      _translateInfobarDelegateObserver;
}

// Overrides superclass property.
@property(nonatomic, readonly)
    translate::TranslateInfoBarDelegate* infoBarDelegate;

@property(nonatomic, weak) TranslateInfobarView* infobarView;

// Indicates whether langugage selection popup menu is offered; and whether it
// is to select the source or the target language.
@property(nonatomic, assign) LanguageSelectionState languageSelectionState;

// Tracks user actions.
@property(nonatomic, assign) UserAction userAction;

// Tracks the total number of translations in a page, incl. reverts to original.
@property(nonatomic, assign) NSUInteger translationsCount;

@end

@implementation TranslateInfoBarController

@dynamic infoBarDelegate;

#pragma mark - InfoBarControllerProtocol

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infoBarDelegate {
  self = [super initWithInfoBarDelegate:infoBarDelegate];
  if (self) {
    _translateInfobarDelegateObserver =
        std::make_unique<TranslateInfobarDelegateObserverBridge>(
            infoBarDelegate, self);
    _userAction = UserActionNone;

    [self recordInfobarEvent:InfobarEvent::INFOBAR_IMPRESSION];
  }
  return self;
}

- (UIView*)infobarView {
  TranslateInfobarView* infobarView =
      [[TranslateInfobarView alloc] initWithFrame:CGRectZero];
  // |_infobarView| is referenced inside |-updateUIForTranslateStep:|.
  _infobarView = infobarView;
  infobarView.sourceLanguage = self.sourceLanguage;
  infobarView.targetLanguage = self.targetLanguage;
  infobarView.delegate = self;
  [self updateUIForTranslateStep:self.infoBarDelegate->translate_step()];
  return infobarView;
}

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  [self updateUIForTranslateStep:step];

  if (step == translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR ||
      step == translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE) {
    [self incrementAndRecordTranslationsCount];
  }
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  return self.userAction == UserActionNone;
}

#pragma mark - TranslateInfobarViewDelegate

- (void)translateInfobarViewDidTapSourceLangugage:
    (TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionRevert;

  [self recordInfobarEvent:InfobarEvent::INFOBAR_REVERT];
  [self incrementAndRecordTranslationsCount];

  self.infoBarDelegate->RevertWithoutClosingInfobar();
  _infobarView.state = TranslateInfobarViewStateBeforeTranslate;
}

- (void)translateInfobarViewDidTapTargetLangugage:
    (TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionTranslate;

  [self recordInfobarEvent:InfobarEvent::INFOBAR_TARGET_TAB_TRANSLATE];
  [self
      recordLanguageDataHistogram:kLanguageHistogramTranslate
                     languageCode:self.infoBarDelegate->target_language_code()];

  if (self.infoBarDelegate->ShouldAutoAlwaysTranslate()) {
    [self recordInfobarEvent:InfobarEvent::
                                 INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION];

    // Page will be translated once the snackbar finishes showing.
    [self.translateNotificationHandler
        showTranslateNotificationWithDelegate:self
                             notificationType:
                                 TranslateNotificationTypeAutoAlwaysTranslate];
  } else {
    self.infoBarDelegate->Translate();
  }
}

- (void)translateInfobarViewDidTapOptions:(TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [self recordInfobarEvent:InfobarEvent::INFOBAR_OPTIONS];

  [self showTranslateOptionSelector];
}

- (void)translateInfobarViewDidTapDismiss:(TranslateInfobarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  if (self.userAction == UserActionNone) {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_DECLINE];
  }

  if (self.infoBarDelegate->ShouldAutoNeverTranslate()) {
    [self recordInfobarEvent:InfobarEvent::
                                 INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION];

    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showTranslateNotificationWithDelegate:self
                             notificationType:
                                 TranslateNotificationTypeAutoNeverTranslate];
  } else {
    self.infoBarDelegate->InfoBarDismissed();
    self.delegate->RemoveInfoBar();
  }
}

#pragma mark - LanguageSelectionDelegate

- (void)languageSelectorSelectedLanguage:(std::string)languageCode {
  if (self.languageSelectionState == LanguageSelectionStateSource) {
    [self recordLanguageDataHistogram:kLanguageHistogramPageNotInLanguage
                         languageCode:languageCode];

    self.infoBarDelegate->UpdateOriginalLanguage(languageCode);
    _infobarView.sourceLanguage = self.sourceLanguage;
  } else {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_MORE_LANGUAGES_TRANSLATE];
    [self recordLanguageDataHistogram:kLanguageHistogramMoreLanguages
                         languageCode:languageCode];

    self.infoBarDelegate->UpdateTargetLanguage(languageCode);
    _infobarView.targetLanguage = self.targetLanguage;
  }
  self.languageSelectionState = LanguageSelectionStateNone;

  self.infoBarDelegate->Translate();

  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

- (void)languageSelectorClosedWithoutSelection {
  self.languageSelectionState = LanguageSelectionStateNone;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

#pragma mark - TranslateOptionSelectionDelegate

- (void)popupMenuTableViewControllerDidSelectTargetLanguageSelector:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [self recordInfobarEvent:InfobarEvent::INFOBAR_MORE_LANGUAGES];

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  self.languageSelectionState = LanguageSelectionStateTarget;
  [self showLanguageSelector];
}

- (void)popupMenuTableViewControllerDidSelectAlwaysTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionAlwaysTranslate;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (self.infoBarDelegate->ShouldAlwaysTranslate()) {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_ALWAYS_TRANSLATE_UNDO];

    self.infoBarDelegate->ToggleAlwaysTranslate();
  } else {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_ALWAYS_TRANSLATE];
    [self recordInfobarEvent:InfobarEvent::
                                 INFOBAR_SNACKBAR_ALWAYS_TRANSLATE_IMPRESSION];
    [self recordLanguageDataHistogram:kLanguageHistogramAlwaysTranslate
                         languageCode:self.infoBarDelegate
                                          ->original_language_code()];

    // Page will be translated once the snackbar finishes showing.
    [self.translateNotificationHandler
        showTranslateNotificationWithDelegate:self
                             notificationType:
                                 TranslateNotificationTypeAlwaysTranslate];
  }
}

- (void)popupMenuTableViewControllerDidSelectNeverTranslateSourceLanguage:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionNeverTranslateLanguage;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (self.infoBarDelegate->IsTranslatableLanguageByPrefs()) {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_NEVER_TRANSLATE];
    [self recordInfobarEvent:InfobarEvent::
                                 INFOBAR_SNACKBAR_NEVER_TRANSLATE_IMPRESSION];
    [self recordLanguageDataHistogram:kLanguageHistogramNeverTranslate
                         languageCode:self.infoBarDelegate
                                          ->original_language_code()];

    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showTranslateNotificationWithDelegate:self
                             notificationType:
                                 TranslateNotificationTypeNeverTranslate];
  }
}

- (void)popupMenuTableViewControllerDidSelectNeverTranslateSite:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionNeverTranslateSite;

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  if (!self.infoBarDelegate->IsSiteBlacklisted()) {
    [self recordInfobarEvent:InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE];
    [self recordInfobarEvent:
              InfobarEvent::INFOBAR_SNACKBAR_NEVER_TRANSLATE_SITE_IMPRESSION];

    // Infobar will dismiss once the snackbar finishes showing.
    [self.translateNotificationHandler
        showTranslateNotificationWithDelegate:self
                             notificationType:
                                 TranslateNotificationTypeNeverTranslateSite];
  }
}

- (void)popupMenuTableViewControllerDidSelectSourceLanguageSelector:
    (PopupMenuTableViewController*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.userAction |= UserActionExpandMenu;

  [self recordInfobarEvent:InfobarEvent::INFOBAR_PAGE_NOT_IN];

  [_infobarView updateUIForPopUpMenuDisplayed:NO];

  self.languageSelectionState = LanguageSelectionStateSource;
  [self showLanguageSelector];
}

- (void)popupMenuPresenterDidCloseWithoutSelection:(PopupMenuPresenter*)sender {
  [_infobarView updateUIForPopUpMenuDisplayed:NO];
}

#pragma mark - TranslateNotificationDelegate

- (void)translateNotificationHandlerDidDismiss:
            (id<TranslateNotificationHandler>)sender
                              notificationType:(TranslateNotificationType)type {
  switch (type) {
    case TranslateNotificationTypeAlwaysTranslate:
    case TranslateNotificationTypeAutoAlwaysTranslate:
      self.infoBarDelegate->ToggleAlwaysTranslate();
      self.infoBarDelegate->Translate();
      break;
    case TranslateNotificationTypeAutoNeverTranslate:
      self.infoBarDelegate->InfoBarDismissed();
      FALLTHROUGH;
    case TranslateNotificationTypeNeverTranslate:
      self.infoBarDelegate->ToggleTranslatableLanguageByPrefs();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateNotificationTypeNeverTranslateSite:
      self.infoBarDelegate->ToggleSiteBlacklist();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateNotificationTypeError:
      // No-op.
      break;
  }
}

- (void)translateNotificationHandlerDidUndo:
            (id<TranslateNotificationHandler>)sender
                           notificationType:(TranslateNotificationType)type {
  switch (type) {
    case TranslateNotificationTypeAlwaysTranslate:
      [self recordInfobarEvent:InfobarEvent::INFOBAR_SNACKBAR_CANCEL_ALWAYS];
      break;
    case TranslateNotificationTypeAutoAlwaysTranslate:
      [self
          recordInfobarEvent:InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS];
      break;
    case TranslateNotificationTypeNeverTranslate:
      [self recordInfobarEvent:InfobarEvent::INFOBAR_SNACKBAR_CANCEL_NEVER];
      break;
    case TranslateNotificationTypeAutoNeverTranslate:
      [self
          recordInfobarEvent:InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER];
      // Remove the infobar even if the user tapped "Undo" since user explicitly
      // dismissed the infobar.
      self.infoBarDelegate->InfoBarDismissed();
      self.delegate->RemoveInfoBar();
      break;
    case TranslateNotificationTypeNeverTranslateSite:
      [self
          recordInfobarEvent:InfobarEvent::INFOBAR_SNACKBAR_CANCEL_NEVER_SITE];
      break;
    case TranslateNotificationTypeError:
      // No-op.
      break;
  }
}

- (NSString*)sourceLanguage {
  return base::SysUTF16ToNSString(
      self.infoBarDelegate->original_language_name());
}

- (NSString*)targetLanguage {
  return base::SysUTF16ToNSString(self.infoBarDelegate->target_language_name());
}

#pragma mark - Private

// Updates the infobar view state for the given translate::TranslateStep. Shows
// an error for translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR.
- (void)updateUIForTranslateStep:(translate::TranslateStep)step {
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      [self.translateNotificationHandler
          showTranslateNotificationWithDelegate:self
                               notificationType:TranslateNotificationTypeError];
      FALLTHROUGH;
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      _infobarView.state = TranslateInfobarViewStateBeforeTranslate;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      _infobarView.state = TranslateInfobarViewStateTranslating;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      _infobarView.state = TranslateInfobarViewStateAfterTranslate;
      break;
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED() << "Translate infobar should never be in this state.";
      break;
  }
}

- (void)showTranslateOptionSelector {
  [self.translateOptionSelectionHandler
      showTranslateOptionSelectorWithInfoBarDelegate:self.infoBarDelegate
                                            delegate:self];
  [_infobarView updateUIForPopUpMenuDisplayed:YES];
}

- (void)showLanguageSelector {
  int originalLanguageIndex = -1;
  int targetLanguageIndex = -1;
  for (size_t i = 0; i < self.infoBarDelegate->num_languages(); ++i) {
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->original_language_code()) {
      originalLanguageIndex = i;
    }
    if (self.infoBarDelegate->language_code_at(i) ==
        self.infoBarDelegate->target_language_code()) {
      targetLanguageIndex = i;
    }
  }
  DCHECK_GE(originalLanguageIndex, 0);
  DCHECK_GE(targetLanguageIndex, 0);

  size_t selectedIndex;
  size_t disabledIndex;
  if (self.languageSelectionState == LanguageSelectionStateSource) {
    selectedIndex = originalLanguageIndex;
    disabledIndex = targetLanguageIndex;
  } else {
    selectedIndex = targetLanguageIndex;
    disabledIndex = originalLanguageIndex;
  }
  LanguageSelectionContext* context =
      [LanguageSelectionContext contextWithLanguageData:self.infoBarDelegate
                                           initialIndex:selectedIndex
                                       unavailableIndex:disabledIndex];
  [self.languageSelectionHandler showLanguageSelectorWithContext:context
                                                        delegate:self];
  [_infobarView updateUIForPopUpMenuDisplayed:YES];
}

- (void)recordInfobarEvent:(InfobarEvent)event {
  UMA_HISTOGRAM_ENUMERATION(kEventHistogram, event);
}

- (void)recordLanguageDataHistogram:(const std::string&)histogram
                       languageCode:(const std::string&)langCode {
  base::SparseHistogram::FactoryGet(
      histogram, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(metrics::MetricsLog::Hash(langCode));
}

- (void)incrementAndRecordTranslationsCount {
  UMA_HISTOGRAM_COUNTS_1M(kTranslationCountHistogram, ++self.translationsCount);
}

@end
