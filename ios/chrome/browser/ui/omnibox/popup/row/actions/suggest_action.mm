// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"

#import "url/gurl.h"

@implementation SuggestAction

+ (instancetype)actionWithOmniboxActionInSuggest:
    (OmniboxActionInSuggest*)cppAction {
  return [[self alloc] initWithAction:cppAction];
}

+ (instancetype)actionWithOmniboxAction:(OmniboxAction*)action {
  auto* actionInSuggest = OmniboxActionInSuggest::FromAction(action);
  if (actionInSuggest) {
    return [self actionWithOmniboxActionInSuggest:actionInSuggest];
  }
  return nil;
}

- (instancetype)initWithAction:(OmniboxActionInSuggest*)action {
  DCHECK(action);
  self = [super init];
  if (self) {
    _type = action->Type();
    _actionURI = GURL(action->action_info.action_uri());
  }
  return self;
}

- (NSString*)title {
  // TODO(crbug.com/331344638) Add translation strings.
  switch (self.type) {
    case omnibox::ActionInfo_ActionType_CALL:
      return @"Call";
    case omnibox::ActionInfo_ActionType_DIRECTIONS:
      return @"Directions";
    case omnibox::ActionInfo_ActionType_REVIEWS:
      return @"Reviews";
    default:
      return nil;
  }
}

@end
