// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_INTERNAL_H_

#import "ios/web_view/public/cwv_web_view.h"

NS_ASSUME_NONNULL_BEGIN

namespace web {
class WebState;
}

@interface CWVWebView ()

// Returns CWVWebView which corresponds to the given web state.
// It causes an assertion failure if the web state has no corresponding
// CWVWebView.
//
// TODO(crbug.com/896961): Write unit test for this method.
+ (CWVWebView*)webViewForWebState:(web::WebState*)webState;

// This is called by the associated CWVWebViewConfiguration in order to shut
// down cleanly. See CWVWebViewConfiguration's |shutDown| method for more info.
- (void)shutDown;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_WEB_VIEW_INTERNAL_H_
