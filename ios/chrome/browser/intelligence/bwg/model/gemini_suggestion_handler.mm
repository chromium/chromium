// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

@implementation GeminiSuggestionHandler {
  // The associated WebStateList.
  raw_ptr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - GeminiSuggestionDelegate

- (void)fetchZeroStateSuggestions:
    (void (^)(NSArray<NSString*>* suggestions))completion {
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    completion(nil);
    return;
  }

  BwgTabHelper* tabHelper = BwgTabHelper::FromWebState(webState);
  if (!tabHelper) {
    completion(nil);
    return;
  }

  tabHelper->ExecuteZeroStateSuggestions(
      base::BindOnce(^(NSArray<NSString*>* suggestions) {
        completion(suggestions);
      }));
}

@end
