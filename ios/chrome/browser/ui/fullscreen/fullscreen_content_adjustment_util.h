// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTENT_ADJUSTMENT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTENT_ADJUSTMENT_UTIL_H_

#import <Foundation/Foundation.h>

@protocol CRWWebViewProxy;
class FullscreenModel;

// Updates `proxy`'s content offset and top padding to ensure that the content
// is fully visible under the header.
void MoveContentBelowHeader(id<CRWWebViewProxy> proxy, FullscreenModel* model);

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTENT_ADJUSTMENT_UTIL_H_
