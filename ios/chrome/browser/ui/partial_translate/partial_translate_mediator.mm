// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_member.h"
#import "components/strings/grit/components_strings.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_utils.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/partial_translate/partial_translate_api.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
typedef void (^ProceduralBlockWithItemArray)(NSArray<UIMenuElement*>*);
typedef void (^ProceduralBlockWithBlockWithItemArray)(
    ProceduralBlockWithItemArray);

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

// Character limit for the partial translate feature.
// A string longer than that will trigger a full page translate.
const NSUInteger kPartialTranslateCharactersLimit = 1000;

}  // anonymous namespace

@interface PartialTranslateMediator ()

// The base view controller to present UI.
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

  // The fullscreen controller to offset sourceRect depending on fullscreen
  // status.
  raw_ptr<FullscreenController> _fullscreenController;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              withBaseViewController:(UIViewController*)baseViewController
                         prefService:(PrefService*)prefs
                fullscreenController:(FullscreenController*)fullscreenController
                           incognito:(BOOL)incognito {
  if ((self = [super init])) {
    DCHECK(webStateList);
    DCHECK(baseViewController);
    _webStateList = webStateList->AsWeakPtr();
    _baseViewController = baseViewController;
    _fullscreenController = fullscreenController;
    _incognito = incognito;
    _translateEnabled.Init(translate::prefs::kOfferTranslateEnabled, prefs);
  }
  return self;
}

- (void)shutdown {
  _translateEnabled.Destroy();
  _fullscreenController = nullptr;
}

- (void)handlePartialTranslateSelection {
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
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper) {
    return NO;
  }
  return tabHelper->CanRetrieveSelectedText() &&
         ios::provider::PartialTranslateLimitMaxCharacters() > 0u;
}

- (BOOL)shouldInstallPartialTranslate {
  if (ios::provider::PartialTranslateLimitMaxCharacters() == 0u) {
    // Feature is not available.
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
                  style:UIAlertActionStyleCancel
              preferred:NO];
  EditMenuAlertDelegateAction* translateAction = [[EditMenuAlertDelegateAction
      alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PARTIAL_TRANSLATE_ACTION_TRANSLATE_FULL_PAGE)
             action:^{
               ReportErrorOutcome(error, true);
               [weakSelf triggerFullTranslate];
             }
              style:UIAlertActionStyleDefault
          preferred:YES];

  [self.alertDelegate
      showAlertWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_PARTIAL_TRANSLATE_SWITCH_FULL_PAGE_TRANSLATION)
                 message:message
                 actions:@[ cancelAction, translateAction ]];
}

- (void)receivedWebSelectionResponse:(WebSelectionResponse*)response {
  DCHECK(response);
  base::UmaHistogramCounts10000("IOS.PartialTranslate.SelectionLength",
                                response.selectedText.length);
  if (response.selectedText.length >
      std::min(ios::provider::PartialTranslateLimitMaxCharacters(),
               kPartialTranslateCharactersLimit)) {
    return [self switchToFullTranslateWithError:PartialTranslateError::
                                                    kSelectionTooLong];
  }
  if (!response.valid ||
      [[response.selectedText
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u) {
    return [self
        switchToFullTranslateWithError:PartialTranslateError::kSelectionEmpty];
  }

  CGRect sourceRect = response.sourceRect;
  if (_fullscreenController && !CGRectEqualToRect(sourceRect, CGRectZero)) {
    UIEdgeInsets fullscreenInset =
        _fullscreenController->GetCurrentViewportInsets();
    sourceRect.origin.y += fullscreenInset.top;
    sourceRect.origin.x += fullscreenInset.left;
  }

  self.controller = ios::provider::NewPartialTranslateController(
      response.selectedText, sourceRect, self.incognito);
  __weak __typeof(self) weakSelf = self;
  [self.controller presentOnViewController:self.baseViewController
                     flowCompletionHandler:^(BOOL success) {
                       weakSelf.controller = nil;
                       if (success) {
                         ReportOutcome(PartialTranslateOutcomeStatus::kSuccess);
                       } else {
                         [weakSelf switchToFullTranslateWithError:
                                       PartialTranslateError::kGenericError];
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

- (void)addItemWithCompletion:(ProceduralBlockWithItemArray)completion {
  if (![self canHandlePartialTranslateSelection]) {
    completion(@[]);
    return;
  }
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper) {
    completion(@[]);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf) {
      [weakSelf addItemWithResponse:response completion:completion];
    } else {
      completion(@[]);
    }
  }));
}

- (void)addItemWithResponse:(WebSelectionResponse*)response
                 completion:(ProceduralBlockWithItemArray)completion {
  __weak __typeof(self) weakSelf = self;
  if (!response.valid ||
      [[response.selectedText
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u) {
    completion(@[]);
    return;
  }
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_EDIT_MENU_ENTRY);
  NSString* partialTranslateId = @"chromecommand.partialTranslate";
  UIAction* action =
      [UIAction actionWithTitle:title
                          image:CustomSymbolWithPointSize(
                                    kTranslateSymbol, kSymbolActionPointSize)
                     identifier:partialTranslateId
                        handler:^(UIAction* a) {
                          [weakSelf receivedWebSelectionResponse:response];
                        }];
  completion(@[ action ]);
}

#pragma mark - EditMenuProvider

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  if (![self shouldInstallPartialTranslate]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithBlockWithItemArray provider =
      ^(ProceduralBlockWithItemArray completion) {
        [weakSelf addItemWithCompletion:completion];
      };
  // Use a deferred element so that the item is displayed depending on the text
  // selection and updated on selection change.
  UIDeferredMenuElement* deferredMenuElement =
      [UIDeferredMenuElement elementWithProvider:provider];
  edit_menu::AddElementToChromeMenu(builder, deferredMenuElement);

  auto childrenTransformBlock =
      ^NSArray<UIMenuElement*>*(NSArray<UIMenuElement*>* oldElements) {
    return [oldElements
        filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(
                             id object, NSDictionary<NSString*, id>* bindings) {
              if (![object isKindOfClass:[UICommand class]]) {
                return YES;
              }
              UICommand* command = base::apple::ObjCCast<UICommand>(object);
              return command.action != NSSelectorFromString(@"_translate:");
            }]];
  };

  [builder replaceChildrenOfMenuForIdentifier:UIMenuLookup
                            fromChildrenBlock:childrenTransformBlock];
}

@end
