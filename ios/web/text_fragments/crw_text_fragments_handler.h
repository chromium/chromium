// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEXT_FRAGMENTS_CRW_TEXT_FRAGMENTS_HANDLER_H_
#define IOS_WEB_TEXT_FRAGMENTS_CRW_TEXT_FRAGMENTS_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/crw_web_view_handler.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class NavigationContext;
struct Referrer;
}

// Class in charge of highlighting text fragments when they are present in
// WebStates' loaded URLs.
@interface CRWTextFragmentsHandler : CRWWebViewHandler

// Initializes a handler with a |delegate| to retrieve the current WebState.
- (instancetype)initWithDelegate:(id<CRWWebViewHandlerDelegate>)delegate;

// Checks the WebState's destination URL for Text Fragments. If found, searches
// the DOM for matching text, highlights the text, and scrolls the first into
// view. Uses the |context| and |referrer| to analyze the current navigation
// scenario.
- (void)processTextFragmentsWithContext:(web::NavigationContext*)context
                               referrer:(web::Referrer)referrer;

@end

#endif  // IOS_WEB_TEXT_FRAGMENTS_CRW_TEXT_FRAGMENTS_HANDLER_H_
