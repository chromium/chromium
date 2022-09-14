// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_ELEMENT_FETCHER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_ELEMENT_FETCHER_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {
struct ContextMenuParams;
class WebState;
}

// Class handling the fetching information about DOM element in a specific
// position.
@interface CRWContextMenuElementFetcher : NSObject

- (instancetype)initWithWebView:(WKWebView*)webView
                       webState:(web::WebState*)webState;

// Asynchronously fetches information about DOM element for the given `point`
// (in the scroll view coordinates). `handler` can not be nil.
- (void)fetchDOMElementAtPoint:(CGPoint)point
             completionHandler:(void (^)(const web::ContextMenuParams&))handler;

// Cancels all the fetches current in progress.
- (void)cancelFetches;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_CONTEXT_MENU_ELEMENT_FETCHER_H_
