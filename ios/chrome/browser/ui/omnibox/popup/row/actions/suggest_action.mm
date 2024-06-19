// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

+ (UIImage*)imageIconForAction:(SuggestAction*)suggestAction
                          size:(CGFloat)size {
  switch (suggestAction.type) {
    case omnibox::ActionInfo_ActionType_CALL:
      return DefaultSymbolWithPointSize(kPhoneFillSymbol, size);
    case omnibox::ActionInfo_ActionType_DIRECTIONS:
      return DefaultSymbolWithPointSize(kTurnUpRightDiamondFillSymbol, size);
    case omnibox::ActionInfo_ActionType_REVIEWS:
      return DefaultSymbolWithPointSize(kStarBubbleFillSymbol, size);
    default:
      return nil;
  }
}

+ (NSString*)accessibilityIdentifierWithType:
                 (omnibox::ActionInfo::ActionType)type
                                 highlighted:(BOOL)highlighted {
  if (type == omnibox::ActionInfo_ActionType_CALL) {
    return highlighted ? kCallActionHighlightedIdentifier
                       : kCallActionIdentifier;
  } else if (type == omnibox::ActionInfo_ActionType_DIRECTIONS) {
    return highlighted ? kDirectionsActionHighlightedIdentifier
                       : kDirectionsActionIdentifier;
  } else if (type == omnibox::ActionInfo_ActionType_REVIEWS) {
    return highlighted ? kReviewsActionHighlightedIdentifier
                       : kReviewsActionIdentifier;
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
  switch (self.type) {
    case omnibox::ActionInfo_ActionType_CALL:
      return l10n_util::GetNSString(IDS_IOS_CALL_OMNIBOX_ACTION);
    case omnibox::ActionInfo_ActionType_DIRECTIONS:
      return l10n_util::GetNSString(IDS_IOS_DIRECTIONS_OMNIBOX_ACTION);
    case omnibox::ActionInfo_ActionType_REVIEWS:
      return l10n_util::GetNSString(IDS_IOS_REVIEWS_OMNIBOX_ACTION);
    default:
      return nil;
  }
}

@end
