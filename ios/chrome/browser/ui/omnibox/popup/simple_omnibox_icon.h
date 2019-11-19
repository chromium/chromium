// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SIMPLE_OMNIBOX_ICON_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SIMPLE_OMNIBOX_ICON_H_

#import "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon.h"

class GURL;

@interface SimpleOmniboxIcon : NSObject <OmniboxIcon>

- (instancetype)initWithIconType:(OmniboxIconType)iconType
              suggestionIconType:(OmniboxSuggestionIconType)suggestionIconType
                        isAnswer:(BOOL)isAnswer
                        imageURL:(GURL)imageURL NS_DESIGNATED_INITIALIZER;

// Whether the default search engine is Google impacts which icon is used in
// some cases
@property(nonatomic, assign) BOOL defaultSearchEngineIsGoogle;

@property(nonatomic, assign) BOOL incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SIMPLE_OMNIBOX_ICON_H_
