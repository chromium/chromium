// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_IMAGE_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_IMAGE_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;
class GURL;

using FaviconCompletionHandler = void (^)(FaviconAttributes*);

// Protocol used by content suggests to retrieve favicons.
@protocol ContentSuggestionsImageDataSource

// Requests the receiver to provide a favicon image for `URL`. A `completion` is
// called asynchronously with a FaviconAttributes instance if appropriate.
- (void)fetchFaviconForURL:(const GURL&)URL
                completion:(FaviconCompletionHandler)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_IMAGE_DATA_SOURCE_H_
