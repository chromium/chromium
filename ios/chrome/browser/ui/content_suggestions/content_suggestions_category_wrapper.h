// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CATEGORY_WRAPPER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CATEGORY_WRAPPER_H_

#import <Foundation/Foundation.h>

#include "components/ntp_snippets/category.h"

// Objective-C wrapper for ntp_snippets::Category.
@interface ContentSuggestionsCategoryWrapper : NSObject<NSCopying>

+ (ContentSuggestionsCategoryWrapper*)wrapperWithCategory:
    (ntp_snippets::Category)category;

- (instancetype)initWithCategory:(ntp_snippets::Category)category
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The category wrapped by this object.
- (ntp_snippets::Category)category;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CATEGORY_WRAPPER_H_
