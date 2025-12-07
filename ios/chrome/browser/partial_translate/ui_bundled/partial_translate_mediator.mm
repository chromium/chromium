// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/partial_translate/ui_bundled/partial_translate_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_member.h"
#import "components/strings/grit/components_strings.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_utils.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
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

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                               prefService:(PrefService*)prefs
                      fullscreenController:
                          (FullscreenController*)fullscreenController
                                 incognito:(BOOL)incognito {
  if ((self = [super init])) {
    DCHECK(baseViewController);
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

- (void)handlePartialTranslateSelectionForTestingInWebState:
    (web::WebState*)webState {
  if (!webState) {
    return;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return;
  }
  GURL pageURL = webState->GetLastCommittedURL();
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    [weakSelf receivedWebSelectionResponse:response forPageURL:pageURL];
  }));
}

- (BOOL)canHandlePartialTranslateSelectionInWebState:(web::WebState*)webState {
  if (!webState) {
    return NO;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
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

#pragma mark - EditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }
  if (![self shouldInstallPartialTranslate]) {
    return;
  }

  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  __weak __typeof(self) weakSelf = self;

  if (ShouldShowEditMenuItemsSynchronously()) {
    UIAction* action = [self actionWithHandler:^(UIAction* a) {
      [weakSelf fetchSelectionForWebState:weakWebState];
    }];
    edit_menu::AddElementToChromeMenu(builder, action,
                                      /*primary*/ YES);
  } else {
    ProceduralBlockWithBlockWithItemArray provider =
        ^(ProceduralBlockWithItemArray completion) {
          [weakSelf addItemWithCompletion:completion forWebState:weakWebState];
        };
    // Use a deferred element so that the item is displayed depending on the
    // text selection and updated on selection change.
    UIDeferredMenuElement* deferredMenuElement =
        [UIDeferredMenuElement elementWithProvider:provider];
    edit_menu::AddElementToChromeMenu(builder, deferredMenuElement,
                                      /*primary*/ YES);
  }

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

#pragma mark - private

// Returns the action to trigger the partial translate feature. Calls `handler`
// on activation.
- (UIAction*)actionWithHandler:(void (^)(UIAction*))handler {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_EDIT_MENU_ENTRY);
  NSString* partialTranslateId = @"chromecommand.partialTranslate";
  return [UIAction actionWithTitle:title
                             image:CustomSymbolWithPointSize(
                                       kTranslateSymbol, kSymbolActionPointSize)
                        identifier:partialTranslateId
                           handler:handler];
}

- (void)fetchSelectionForWebState:(base::WeakPtr<web::WebState>)weakWebState {
  if (!weakWebState) {
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self canHandlePartialTranslateSelectionInWebState:webState]) {
    return;
  }
  GURL pageURL = webState->GetLastCommittedURL();
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf && response.valid) {
      [weakSelf receivedWebSelectionResponse:response forPageURL:pageURL];
    }
  }));
}

// If the partial translate fails, try to switch to normal page translate.
- (void)switchToFullTranslateWithError:(PartialTranslateError)error
                            forPageURL:(const GURL&)pageURL {
  if (!self.alertDelegate) {
    return;
  }
  if (!TranslateServiceIOS::IsTranslatableURL(pageURL)) {
    ReportErrorOutcome(error, false);
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

// The selection was received from JS. Proceed to check it and trigger partial
// translate.
- (void)receivedWebSelectionResponse:(WebSelectionResponse*)response
                          forPageURL:(const GURL&)pageURL {
  DCHECK(response);
  base::UmaHistogramCounts10000("IOS.PartialTranslate.SelectionLength",
                                response.selectedText.length);
  if (response.selectedText.length >
      std::min(ios::provider::PartialTranslateLimitMaxCharacters(),
               kPartialTranslateCharactersLimit)) {
    return [self
        switchToFullTranslateWithError:PartialTranslateError::kSelectionTooLong
                            forPageURL:pageURL];
  }
  if (!response.valid ||
      [[response.selectedText
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u) {
    return [self
        switchToFullTranslateWithError:PartialTranslateError::kSelectionEmpty
                            forPageURL:pageURL];
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
  GURL copyPageURL = pageURL;
  [self.controller presentOnViewController:self.baseViewController
                     flowCompletionHandler:^(BOOL success) {
                       [weakSelf flowCompletedForPageURL:copyPageURL
                                              withResult:success];
                     }];
}

// Flow ended. If it is a success, report it to metrics. Otherwise, try to
// trigger page translate instead.
- (void)flowCompletedForPageURL:(const GURL&)pageURL withResult:(BOOL)success {
  self.controller = nil;
  if (success) {
    ReportOutcome(PartialTranslateOutcomeStatus::kSuccess);
  } else {
    [self switchToFullTranslateWithError:PartialTranslateError::kGenericError
                              forPageURL:pageURL];
  }
}

// Trigger a full translate as a fallback of partial translate.
- (void)triggerFullTranslate {
  [self.browserHandler showTranslate];
}

// Create the menu item to trigger partial translate step 1.
// This method is called from the UIDeferredElement handler.
// This method triggers the fetch of the selection.
- (void)addItemWithCompletion:(ProceduralBlockWithItemArray)completion
                  forWebState:(base::WeakPtr<web::WebState>)weakWebState {
  if (!weakWebState) {
    completion(@[]);
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self canHandlePartialTranslateSelectionInWebState:webState]) {
    completion(@[]);
    return;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  if (!tabHelper) {
    completion(@[]);
    return;
  }
  GURL pageURL = webState->GetLastCommittedURL();

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf) {
      [weakSelf addItemWithResponse:response
                         forPageURL:pageURL
                         completion:completion];
    } else {
      completion(@[]);
    }
  }));
}

// Create the menu item to trigger partial translate step 2.
// This selection was fetched, if it is valid, add the action in the edit menu.
- (void)addItemWithResponse:(WebSelectionResponse*)response
                 forPageURL:(const GURL&)pageURL
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
  GURL pageURLCopy = pageURL;
  UIAction* action = [self actionWithHandler:^(UIAction* a) {
    [weakSelf receivedWebSelectionResponse:response forPageURL:pageURLCopy];
  }];

  completion(@[ action ]);
}

@end
