// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SIMPLE_OMNIBOX_ICON_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SIMPLE_OMNIBOX_ICON_H_

#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_icon.h"
#import "ios/chrome/browser/omnibox/public/omnibox_suggestion_icon_util.h"

@class CrURL;

@interface SimpleOmniboxIcon : NSObject <OmniboxIcon>

- (instancetype)initWithIconType:(OmniboxIconType)iconType
              suggestionIconType:(OmniboxSuggestionIconType)suggestionIconType
                        isAnswer:(BOOL)isAnswer
                        imageURL:(CrURL*)imageURL NS_DESIGNATED_INITIALIZER;

/// Whether the default search engine is Google impacts which icon is used in
/// some cases
@property(nonatomic, assign) BOOL defaultSearchEngineIsGoogle;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_SIMPLE_OMNIBOX_ICON_H_
