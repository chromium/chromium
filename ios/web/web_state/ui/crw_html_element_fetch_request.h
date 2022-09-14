// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_HTML_ELEMENT_FETCH_REQUEST_H_
#define IOS_WEB_WEB_STATE_UI_CRW_HTML_ELEMENT_FETCH_REQUEST_H_

#import <Foundation/Foundation.h>

namespace base {
class TimeTicks;
}  // namespace base

namespace web {
struct ContextMenuParams;
}

// Tracks request details for fetching attributes of an element.
@interface CRWHTMLElementFetchRequest : NSObject

// The time this object was created.
@property(nonatomic, readonly) base::TimeTicks creationTime;

- (instancetype)init NS_UNAVAILABLE;
// Designated initializer to create a new object with the given completion
// handler `foundElementHandler`.
- (instancetype)initWithFoundElementHandler:
    (void (^)(const web::ContextMenuParams&))foundElementHandler
    NS_DESIGNATED_INITIALIZER;

// Calls the `foundElementHandler` from the receiver's initializer with
// `response` as the parameter. This method has no effect if `invalidate` has
// been called.
- (void)runHandlerWithResponse:(const web::ContextMenuParams&)response;
// Removes the stored `foundElementHandler` from the receiver's initializer.
// `runHandlerWithResponse:` will have no effect if called after `invalidate`.
- (void)invalidate;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_HTML_ELEMENT_FETCH_REQUEST_H_
