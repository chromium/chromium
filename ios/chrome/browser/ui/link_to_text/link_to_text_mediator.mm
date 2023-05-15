// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/link_to_text_response.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::LinkGenerationError;

@implementation LinkToTextMediator {
  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if (self = [super init]) {
    DCHECK(webStateList);
    _webStateList = webStateList->AsWeakPtr();
  }
  return self;
}

- (void)dealloc {
  if (_webStateList) {
    _webStateList = nullptr;
  }
}

- (BOOL)shouldOfferLinkToText {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  LinkToTextTabHelper* tabHelper = [self linkToTextTabHelper];
  if (!tabHelper) {
    return NO;
  }
  return tabHelper->ShouldOffer();
}

- (void)handleLinkToTextSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  LinkToTextTabHelper* tabHelper = [self linkToTextTabHelper];
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
    shared_highlighting::LogLinkGeneratedErrorUkmEvent(response.sourceID,
                                                       error);
    shared_highlighting::LogGenerateErrorLatency(response.latency);
    [self linkGenerationFailedWithError:error];
  } else {
    shared_highlighting::LogLinkGeneratedSuccessUkmEvent(response.sourceID);
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
  [self.activityServiceHandler shareHighlight:command];
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
                   [weakSelf.activityServiceHandler sharePage];
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

- (LinkToTextTabHelper*)linkToTextTabHelper {
  web::WebState* webState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!webState) {
    return nullptr;
  }
  LinkToTextTabHelper* helper = LinkToTextTabHelper::FromWebState(webState);
  DCHECK(helper);
  return helper;
}

@end
