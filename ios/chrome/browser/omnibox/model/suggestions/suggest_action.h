// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SUGGEST_ACTION_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SUGGEST_ACTION_H_

#import <UIKit/UIKit.h>

#import "components/omnibox/browser/actions/omnibox_action_in_suggest.h"

class GURL;

// A UIKit wrapper for OmniboxActionInSuggest.
@interface SuggestAction : NSObject

- (instancetype)initWithAction:(OmniboxActionInSuggest*)action
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)actionWithOmniboxActionInSuggest:
    (OmniboxActionInSuggest*)cppAction;

// Returns a suggestAction if the omniboxAction is an action in suggest, nil
// otherwise.
+ (instancetype)actionWithOmniboxAction:(OmniboxAction*)action;

// Returns an image icon for the given action.
+ (UIImage*)imageIconForAction:(SuggestAction*)action size:(CGFloat)size;

// Whether the given OmniboxActionInSuggest is supported by this class.
+ (BOOL)isActionSupported:(OmniboxActionInSuggest*)action;

// Returns the accessibility identifier for an action based on its type and
// highlighting state.
+ (NSString*)accessibilityIdentifierWithType:
                 (omnibox::SuggestTemplateInfo_TemplateAction_ActionType)type
                                 highlighted:(BOOL)highlighted;

// The suggestion action uri.
@property(nonatomic, assign, readonly) GURL actionURI;
// The suggestion action title.
@property(nonatomic, copy, readonly) NSString* title;
// The suggestion action type.
@property(nonatomic, assign, readonly)
    omnibox::SuggestTemplateInfo_TemplateAction_ActionType type;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SUGGEST_ACTION_H_
