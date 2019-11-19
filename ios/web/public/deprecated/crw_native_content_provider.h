// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_PROVIDER_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_PROVIDER_H_
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

// Returns the inset from |webState|'s view to lay out provided native content.
- (UIEdgeInsets)nativeContentInsetForWebState:(web::WebState*)webState;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_PROVIDER_H_
