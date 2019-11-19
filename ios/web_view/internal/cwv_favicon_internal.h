// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_FAVICON_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_FAVICON_INTERNAL_H_

#include <vector>
#include "ios/web/public/favicon/favicon_url.h"
#import "ios/web_view/public/cwv_favicon.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVFavicon ()

// Use to convert a vector of web::FaviconURL into a NSArray of CWVFavicon.
+ (NSArray<CWVFavicon*>*)faviconsFromFaviconURLs:
    (const std::vector<web::FaviconURL>&)faviconURLs;

- (instancetype)initWithFaviconURL:(web::FaviconURL)faviconURL
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_FAVICON_INTERNAL_H_
