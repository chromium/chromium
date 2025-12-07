// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/suggest_action.h"

#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation SuggestAction

+ (instancetype)actionWithOmniboxActionInSuggest:
    (OmniboxActionInSuggest*)cppAction {
  DCHECK(cppAction && [self.class isActionSupported:cppAction]);
  if (cppAction && [self.class isActionSupported:cppAction]) {
    return [[self alloc] initWithAction:cppAction];
  }
  return nil;
}

+ (instancetype)actionWithOmniboxAction:(OmniboxAction*)action {
  auto* actionInSuggest = OmniboxActionInSuggest::FromAction(action);
  if (actionInSuggest && [self.class isActionSupported:actionInSuggest]) {
    return [self actionWithOmniboxActionInSuggest:actionInSuggest];
  }
  return nil;
}

+ (BOOL)isActionSupported:(OmniboxActionInSuggest*)action {
  CHECK(action);
  switch (action->Type()) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM:
      return YES;
    default:
      return NO;
  }
}

+ (UIImage*)imageIconForAction:(SuggestAction*)suggestAction
                          size:(CGFloat)size {
  switch (suggestAction.type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return DefaultSymbolWithPointSize(kPhoneFillSymbol, size);
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return DefaultSymbolWithPointSize(kTurnUpRightDiamondFillSymbol, size);
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return DefaultSymbolWithPointSize(kStarBubbleFillSymbol, size);
    default:
      return nil;
  }
}

+ (NSString*)accessibilityIdentifierWithType:
                 (omnibox::SuggestTemplateInfo_TemplateAction_ActionType)type
                                 highlighted:(BOOL)highlighted {
  if (type == omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL) {
    return highlighted ? kCallActionHighlightedIdentifier
                       : kCallActionIdentifier;
  } else if (type ==
             omnibox::
                 SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS) {
    return highlighted ? kDirectionsActionHighlightedIdentifier
                       : kDirectionsActionIdentifier;
  } else if (type ==
             omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS) {
    return highlighted ? kReviewsActionHighlightedIdentifier
                       : kReviewsActionIdentifier;
  }
  return nil;
}

- (instancetype)initWithAction:(OmniboxActionInSuggest*)action {
  CHECK(action);
  CHECK([SuggestAction.class isActionSupported:action]);
  self = [super init];
  if (self) {
    _type = action->Type();
    _actionURI = GURL(action->template_action.action_uri());
  }
  return self;
}

- (NSString*)title {
  switch (self.type) {
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CALL:
      return l10n_util::GetNSString(IDS_IOS_CALL_OMNIBOX_ACTION);
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_DIRECTIONS:
      return l10n_util::GetNSString(IDS_IOS_DIRECTIONS_OMNIBOX_ACTION);
    case omnibox::SuggestTemplateInfo_TemplateAction_ActionType_REVIEWS:
      return l10n_util::GetNSString(IDS_IOS_REVIEWS_OMNIBOX_ACTION);
    default:
      return nil;
  }
}

@end
