// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_member.h"
#import "components/strings/grit/components_strings.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_selection/web_selection_response.h"
#import "ios/chrome/browser/web_selection/web_selection_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum class PartialTranslateError {
  kSelectionTooLong,
  kSelectionEmpty,
  kGenericError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PartialTranslateOutcomeStatus {
  kSuccess,
  kTooLongCancel,
  kTooLongFullTranslate,
  kEmptyCancel,
  kEmptyFullTranslate,
  kErrorCancel,
  kErrorFullTranslate,
  kMaxValue = kErrorFullTranslate
};

void ReportOutcome(PartialTranslateOutcomeStatus outcome) {
  base::UmaHistogramEnumeration("IOS.PartialTranslate.Outcome", outcome);
}

void ReportErrorOutcome(PartialTranslateError error, bool went_full) {
  switch (error) {
    case PartialTranslateError::kSelectionTooLong:
      if (went_full) {
        ReportOutcome(PartialTranslateOutcomeStatus::kTooLongFullTranslate);
      } else {
        ReportOutcome(PartialTranslateOutcomeStatus::kTooLongCancel);
      }
      break;
    case PartialTranslateError::kSelectionEmpty:
      if (went_full) {
        ReportOutcome(PartialTranslateOutcomeStatus::kEmptyFullTranslate);
      } else {
        ReportOutcome(PartialTranslateOutcomeStatus::kEmptyCancel);
      }
      break;
    case PartialTranslateError::kGenericError:
      if (went_full) {
        ReportOutcome(PartialTranslateOutcomeStatus::kErrorFullTranslate);
      } else {
        ReportOutcome(PartialTranslateOutcomeStatus::kErrorCancel);
      }
      break;
  }
}

}  // anonymous namespace

@interface PartialTranslateMediator ()

// Whether the mediator is handling partial translate for an incognito tab.
@property(nonatomic, weak) UIViewController* baseViewController;

// Whether the mediator is handling partial translate for an incognito tab.
@property(nonatomic, assign) BOOL incognito;

// The controller to display Partial Translate.
@property(nonatomic, strong) id<PartialTranslateController> controller;

@end

@implementation PartialTranslateMediator {
  BooleanPrefMember _translateEnabled;

  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(base::WeakPtr<WebStateList>)webStateList
              withBaseViewController:(UIViewController*)baseViewController
                         prefService:(PrefService*)prefs
                           incognito:(BOOL)incognito {
  if (self = [super init]) {
    DCHECK(webStateList);
    DCHECK(baseViewController);
    _webStateList = webStateList;
    _baseViewController = baseViewController;
    _incognito = incognito;
    _translateEnabled.Init(translate::prefs::kOfferTranslateEnabled, prefs);
  }
  return self;
}

- (void)shutdown {
  _translateEnabled.Destroy();
}

- (void)handlePartialTranslateSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    [weakSelf receivedWebSelectionResponse:response];
  }));
}

- (BOOL)canHandlePartialTranslateSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper) {
    return NO;
  }
  return tabHelper->CanRetrieveSelectedText() &&
         PartialTranslateLimitMaxCharacters() > 0u;
}

- (BOOL)shouldInstallPartialTranslate {
  if (PartialTranslateLimitMaxCharacters() == 0u) {
    // Feature is not available.
    return NO;
  }
  if (!base::FeatureList::IsEnabled(kIOSEditMenuPartialTranslate)) {
    // Feature is not enabled.
    return NO;
  }
  if (self.incognito && !ShouldShowPartialTranslateInIncognito()) {
    // Feature is enabled, but disabled in incognito, and the current tab is in
    // incognito.
    return NO;
  }
  if (!_translateEnabled.GetValue() && _translateEnabled.IsManaged()) {
    // Translate is a managed settings and disabled.
    return NO;
  }
  return YES;
}

- (void)switchToFullTranslateWithError:(PartialTranslateError)error {
  if (!self.alertDelegate) {
    return;
  }
  NSString* message;
  switch (error) {
    case PartialTranslateError::kSelectionTooLong:
      message = l10n_util::GetNSString(
          IDS_IOS_PARTIAL_TRANSLATE_ERROR_STRING_TOO_LONG_ERROR);
      break;
    case PartialTranslateError::kSelectionEmpty:
      message =
          l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_ERROR_STRING_EMPTY);
      break;
    case PartialTranslateError::kGenericError:
      message = l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_ERROR_GENERIC);
      break;
  }
  DCHECK(message);
  __weak __typeof(self) weakSelf = self;
  EditMenuAlertDelegateAction* cancelAction =
      [[EditMenuAlertDelegateAction alloc]
          initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                 action:^{
                   ReportErrorOutcome(error, false);
                 }
                  style:UIAlertActionStyleCancel];
  EditMenuAlertDelegateAction* translateAction = [[EditMenuAlertDelegateAction
      alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PARTIAL_TRANSLATE_ACTION_TRANSLATE_FULL_PAGE)
             action:^{
               ReportErrorOutcome(error, true);
               [weakSelf triggerFullTranslate];
             }
              style:UIAlertActionStyleDefault];

  [self.alertDelegate
      showAlertWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PARTIAL_TRANSLATE_SWITCH_FULL_PAGE_TRANSLATION)
                 message:message
                 actions:@[ cancelAction, translateAction ]];
}

- (void)receivedWebSelectionResponse:(WebSelectionResponse*)response {
  DCHECK(response);
  if (response.selectedText.length > PartialTranslateLimitMaxCharacters()) {
    return [self switchToFullTranslateWithError:PartialTranslateError::
                                                    kSelectionTooLong];
  }
  if ([[response.selectedText
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u) {
    return [self
        switchToFullTranslateWithError:PartialTranslateError::kSelectionEmpty];
  }
  __weak __typeof(self) weakSelf = self;
  self.controller = NewPartialTranslateController(
      response.selectedText, response.sourceRect, self.incognito);
  [self.controller
      presentOnViewController:self.baseViewController
        flowCompletionHandler:^(BOOL success) {
          weakSelf.controller = nil;
          if (success) {
            ReportOutcome(PartialTranslateOutcomeStatus::kSuccess);
          } else {
            [weakSelf switchToFullTranslateWithError:PartialTranslateError::
                                                         kGenericError];
          }
        }];
}

- (void)triggerFullTranslate {
  [self.browserHandler showTranslate];
}

- (WebSelectionTabHelper*)webSelectionTabHelper {
  web::WebState* webState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!webState) {
    return nullptr;
  }
  WebSelectionTabHelper* helper = WebSelectionTabHelper::FromWebState(webState);
  return helper;
}

@end
