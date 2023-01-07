// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_H_

#import <Foundation/Foundation.h>

// Handler for the WebView. This class is mostly used to be inherited.
@interface CRWWebViewHandler : NSObject

// Whether the handler is being closed.
@property(nonatomic, assign, getter=isBeingDestroyed, readonly)
    BOOL beingDestroyed;

// Closes the handler.
- (void)close NS_REQUIRES_SUPER;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_H_
