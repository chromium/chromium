// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
#import <UIKit/UIKit.h>

class GURL;

@protocol CRWNativeContent;

namespace web {
class WebState;
}

// Provide a controller to a native view representing a given URL.
@protocol CRWNativeContentProvider

// Returns whether the Provider has a controller for the given URL.
- (BOOL)hasControllerForURL:(const GURL&)url;

// Returns an autoreleased controller for driving a native view contained
// within the web content area. This may return nil if the url is unsupported.
// |url| will be of the form "chrome://foo".
// |webState| is the webState that triggered the navigation to |url|.
- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState;

// Called to retrieve the height of any header that is overlaying on top of the
// native content. This can be used to implement, for e.g. a toolbar that
// changes height dynamically. Returning a non-zero height affects the visible
// frame shown by the CRWWebController. 0.0 is assumed if not implemented.
// TODO(crbug.com/674991) These should be removed when native content is
// removed.
- (CGFloat)nativeContentHeaderHeightForWebState:(web::WebState*)webState;

@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_UI_CRW_NATIVE_CONTENT_PROVIDER_H_
