// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"

#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LinkToTextMediator ()

// The Browser's WebStateList.
@property(nonatomic, readonly) WebStateList* webStateList;

// Instance in charge of handling Activity Service's related commands.
@property(nonatomic, readonly, weak) id<ActivityServiceCommands> handler;

@end

@implementation LinkToTextMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             handler:(id<ActivityServiceCommands>)handler {
  if (self = [super init]) {
    DCHECK(webStateList);
    DCHECK(handler);
    _webStateList = webStateList;
    _handler = handler;
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
  return [self getLinkToTextTabHelper]->ShouldOffer();
}

- (void)handleLinkToTextSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  LinkToTextTabHelper* tabHelper = [self getLinkToTextTabHelper];

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetLinkToText(^(LinkToTextPayload* payload) {
    [weakSelf shareLinkToText:payload];
  });
}

- (void)shareLinkToText:(LinkToTextPayload*)payload {
  ShareHighlightCommand* command =
      [[ShareHighlightCommand alloc] initWithURL:payload.URL
                                           title:payload.title
                                    selectedText:payload.selectedText
                                      sourceView:payload.sourceView
                                      sourceRect:payload.sourceRect];
  [self.handler shareHighlight:command];
}

- (LinkToTextTabHelper*)getLinkToTextTabHelper {
  web::WebState* web_state = _webStateList->GetActiveWebState();
  DCHECK(web_state);
  LinkToTextTabHelper* helper = LinkToTextTabHelper::FromWebState(web_state);
  DCHECK(helper);
  return helper;
}

@end
