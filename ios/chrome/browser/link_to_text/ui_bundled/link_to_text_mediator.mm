// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_response.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

using shared_highlighting::LinkGenerationError;

namespace {
typedef void (^ProceduralBlockWithItemArray)(NSArray<UIMenuElement*>*);
typedef void (^ProceduralBlockWithBlockWithItemArray)(
    ProceduralBlockWithItemArray);
}  // namespace

@implementation LinkToTextMediator

- (BOOL)shouldOfferLinkToTextInWebState:(web::WebState*)webState {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  if (!webState) {
    return NO;
  }
  LinkToTextTabHelper* tabHelper = LinkToTextTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return NO;
  }
  return tabHelper->ShouldOffer();
}

- (void)handleLinkToTextSelectionInWebState:(web::WebState*)webState {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  if (!webState) {
    return;
  }
  LinkToTextTabHelper* tabHelper = LinkToTextTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetLinkToText(base::BindOnce(^(LinkToTextResponse* response) {
    [weakSelf receivedLinkToTextResponse:response];
  }));
}

- (void)receivedLinkToTextResponse:(LinkToTextResponse*)response {
  DCHECK(response);
  if (response.error.has_value()) {
    LinkGenerationError error = response.error.value();
    if (response.sourceID != ukm::kInvalidSourceId) {
      shared_highlighting::LogLinkGeneratedErrorUkmEvent(response.sourceID,
                                                         error);
    }
    shared_highlighting::LogGenerateErrorLatency(response.latency);
    [self linkGenerationFailedWithError:error];
  } else {
    if (response.sourceID != ukm::kInvalidSourceId) {
      shared_highlighting::LogLinkGeneratedSuccessUkmEvent(response.sourceID);
    }
    shared_highlighting::LogGenerateSuccessLatency(response.latency);
    [self shareLinkToText:response.payload];
  }
}

- (void)shareLinkToText:(LinkToTextPayload*)payload {
  DCHECK(payload);
  shared_highlighting::LogLinkGenerationStatus(
      shared_highlighting::LinkGenerationStatus::kSuccess);
  ShareHighlightCommand* command =
      [[ShareHighlightCommand alloc] initWithURL:payload.URL
                                           title:payload.title
                                    selectedText:payload.selectedText
                                      sourceView:payload.sourceView
                                      sourceRect:payload.sourceRect];
  [self.activityServiceHandler showShareSheetForHighlight:command];
}

- (void)linkGenerationFailedWithError:(LinkGenerationError)error {
  if (!self.alertDelegate) {
    return;
  }
  shared_highlighting::LogLinkGenerationStatus(
      shared_highlighting::LinkGenerationStatus::kFailure);
  shared_highlighting::LogLinkGenerationErrorReason(error);

  __weak __typeof(self) weakSelf = self;
  EditMenuAlertDelegateAction* cancelAction =
      [[EditMenuAlertDelegateAction alloc]
          initWithTitle:l10n_util::GetNSString(IDS_APP_OK)
                 action:^{
                   base::RecordAction(base::UserMetricsAction(
                       "SharedHighlights.LinkGenerated.Error.OK"));
                 }
                  style:UIAlertActionStyleCancel
              preferred:NO];
  EditMenuAlertDelegateAction* translateAction =
      [[EditMenuAlertDelegateAction alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_PAGE_BUTTON_LABEL)
                 action:^{
                   base::RecordAction(base::UserMetricsAction(
                       "SharedHighlights.LinkGenerated.Error.SharePage"));
                   [weakSelf.activityServiceHandler showShareSheet];
                 }
                  style:UIAlertActionStyleDefault
              preferred:NO];

  [self.alertDelegate
      showAlertWithTitle:l10n_util::GetNSString(
                             IDS_IOS_LINK_TO_TEXT_ERROR_TITLE)
                 message:l10n_util::GetNSString(
                             IDS_IOS_LINK_TO_TEXT_ERROR_DESCRIPTION)
                 actions:@[ cancelAction, translateAction ]];
}

- (void)addItemWithCompletion:(ProceduralBlockWithItemArray)completion
                  forWebState:(base::WeakPtr<web::WebState>)weakWebState {
  if (!weakWebState) {
    completion(@[]);
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self shouldOfferLinkToTextInWebState:webState]) {
    completion(@[]);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
  NSString* linkToTextId = @"chromecommand.linktotext";
  UIAction* action = [UIAction
      actionWithTitle:title
                image:DefaultSymbolWithPointSize(kHighlighterSymbol,
                                                 kSymbolActionPointSize)
           identifier:linkToTextId
              handler:^(UIAction* a) {
                [weakSelf
                    handleLinkToTextSelectionInWebState:weakWebState.get()];
              }];
  completion(@[ action ]);
}

#pragma mark - EditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  NSString* linkToTextId = @"chromecommand.menu.linktotext";

  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithBlockWithItemArray provider =
      ^(ProceduralBlockWithItemArray completion) {
        [weakSelf addItemWithCompletion:completion forWebState:weakWebState];
      };
  // Use a deferred element so that the item is displayed depending on the text
  // selection and updated on selection change.
  UIDeferredMenuElement* deferredMenuElement =
      [UIDeferredMenuElement elementWithProvider:provider];

  UIMenu* linkToTextMenu = [UIMenu menuWithTitle:@""
                                           image:nil
                                      identifier:linkToTextId
                                         options:UIMenuOptionsDisplayInline
                                        children:@[ deferredMenuElement ]];
  [builder insertChildMenu:linkToTextMenu atEndOfMenuForIdentifier:UIMenuRoot];
}

@end
