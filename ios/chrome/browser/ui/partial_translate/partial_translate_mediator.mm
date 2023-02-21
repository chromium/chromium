// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"

#import "components/strings/grit/components_strings.h"
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

@interface PartialTranslateMediator ()

// The Browser's WebStateList.
@property(nonatomic, readonly) WebStateList* webStateList;

// Whether the mediator is handling partial translate for an incognito tab.
@property(nonatomic, weak) UIViewController* baseViewController;

// Whether the mediator is handling partial translate for an incognito tab.
@property(nonatomic, assign) BOOL incognito;

// The controller to display Partial Translate.
@property(nonatomic, strong) id<PartialTranslateController> controller;

@end

@implementation PartialTranslateMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              withBaseViewController:(UIViewController*)baseViewController
                           incognito:(BOOL)incognito {
  if (self = [super init]) {
    DCHECK(webStateList);
    DCHECK(baseViewController);
    _webStateList = webStateList;
    _baseViewController = baseViewController;
    _incognito = incognito;
  }
  return self;
}

- (void)shutdown {
  if (_webStateList) {
    _webStateList = nullptr;
  }
}

- (void)handlePartialTranslateSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  // TODO(crbug.com/1417238): add metrics
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    [weakSelf receivedWebSelectionResponse:response];
  }));
}

- (void)switchToFullTranslateWithMessage:(NSString*)message {
  // TODO(crbug.com/1417238): add metrics
  if (!self.alertDelegate) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  EditMenuAlertDelegateAction* cancelAction =
      [[EditMenuAlertDelegateAction alloc]
          initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                 action:^{
                   // TODO(crbug.com/1417238): add metrics
                 }
                  style:UIAlertActionStyleCancel];
  EditMenuAlertDelegateAction* translateAction = [[EditMenuAlertDelegateAction
      alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PARTIAL_TRANSLATE_ACTION_TRANSLATE_FULL_PAGE)
             action:^{
               // TODO(crbug.com/1417238): add metrics
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
  // TODO(crbug.com/1417238): add metrics
  if (response.selectedText.length > PartialTranslateLimitMaxCharacters()) {
    return
        [self switchToFullTranslateWithMessage:
                  l10n_util::GetNSString(
                      IDS_IOS_PARTIAL_TRANSLATE_ERROR_STRING_TOO_LONG_ERROR)];
  }
  if ([[response.selectedText
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u) {
    return
        [self switchToFullTranslateWithMessage:
                  l10n_util::GetNSString(
                      IDS_IOS_PARTIAL_TRANSLATE_ERROR_STRING_TOO_LONG_ERROR)];
  }
  __weak __typeof(self) weakSelf = self;
  self.controller = NewPartialTranslateController(
      response.selectedText, response.sourceRect, self.incognito);
  [self.controller
      presentOnViewController:self.baseViewController
        flowCompletionHandler:^(BOOL success) {
          weakSelf.controller = nil;
          if (!success) {
            [weakSelf switchToFullTranslateWithMessage:
                          l10n_util::GetNSString(
                              IDS_IOS_PARTIAL_TRANSLATE_ERROR_GENERIC)];
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
