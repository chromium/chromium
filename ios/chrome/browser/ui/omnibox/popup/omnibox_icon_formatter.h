// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_FORMATTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_FORMATTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/simple_omnibox_icon.h"

struct AutocompleteMatch;

@interface OmniboxIconFormatter : SimpleOmniboxIcon

- (instancetype)initWithMatch:(const AutocompleteMatch&)match;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_ICON_FORMATTER_H_
