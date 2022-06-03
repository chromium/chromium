// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_PROVIDER_H_

// Object providing a header view for the content suggestions.
@protocol ContentSuggestionsHeaderProvider

- (nullable UIView*)headerForWidth:(CGFloat)width
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_PROVIDER_H_
