// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_PENDING_NAVIGATION_INFO_H_
#define IOS_WEB_NAVIGATION_CRW_PENDING_NAVIGATION_INFO_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "net/http/http_response_headers.h"

// A container object for any navigation information that is only available
// during pre-commit delegate callbacks, and thus must be held until the
// navigation commits and the information can be used.
@interface CRWPendingNavigationInfo : NSObject
// The referrer for the page.
@property(nonatomic, copy) NSString* referrer;
// The MIME type for the page.
@property(nonatomic, copy) NSString* MIMEType;
// The navigation type for the load.
@property(nonatomic, assign) WKNavigationType navigationType;
// HTTP request method for the load.
@property(nonatomic, copy) NSString* HTTPMethod;
// HTTP headers.
@property(nonatomic, assign) scoped_refptr<net::HttpResponseHeaders>
    HTTPHeaders;
// Whether the pending navigation has been cancelled by an embedder before the
// navigation is committed.  Cancelled navigations without a cancellation error
// should simply be discarded.
@property(nonatomic, assign) BOOL cancelled;
// The error to display if the pending navigation was cancelled by an embedder
// using PolicyDecision::CancelAndDisplayError().
@property(nonatomic, copy) NSError* cancellationError;
// Whether the navigation was initiated by a user gesture.
@property(nonatomic, assign) BOOL hasUserGesture;
// Whether the navigation had a server redirect.
@property(nonatomic, assign) BOOL unsafeRedirect;
@end

#endif  // IOS_WEB_NAVIGATION_CRW_PENDING_NAVIGATION_INFO_H_
