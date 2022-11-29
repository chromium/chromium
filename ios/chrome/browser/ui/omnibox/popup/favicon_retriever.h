// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_FAVICON_RETRIEVER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_FAVICON_RETRIEVER_H_

class GURL;

/// A favicon retriever.
/// This protocol is intended to be used in Showcase, abstracting out the actual
/// favicon retrieving logic from the View layer.
@protocol FaviconRetriever <NSObject>
/// Fetches favicon given a page URL.
/// `completion` is guaranteed to only be called on main thread, but could be
/// called at any time, even before this returns.
/// It might never be called if the favicon is not available.
/// It might be called multiple times per request.
- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_FAVICON_RETRIEVER_H_
