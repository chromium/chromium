// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_IMAGE_RETRIEVER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_IMAGE_RETRIEVER_H_

class GURL;

@protocol ImageRetriever <NSObject>
- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_IMAGE_RETRIEVER_H_
