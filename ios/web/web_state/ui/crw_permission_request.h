// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_PERMISSION_REQUEST_H_
#define IOS_WEB_WEB_STATE_UI_CRW_PERMISSION_REQUEST_H_

#import <WebKit/WebKit.h>

#import "base/memory/scoped_refptr.h"
#import "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace web {
class WebStateImpl;
}  // namespace web

// Object that manages the permission request; usually the owner.
@protocol CRWPermissionPresenter <NSObject>

// Web state that the permission would take effect on.
- (web::WebStateImpl*)presentingWebState;

@end

// Encapsulation of the permission request from a WKWebView, which displays a
// prompt on the main thread, handles user response, and deals with edge cases.
@interface CRWPermissionRequest : NSObject

// Initializes the request with `decisionHandler` to be executed on `taskRunner`
// once a decision is made.
- (instancetype)
    initWithPresenter:(id<CRWPermissionPresenter>)presenter
      decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler
         onTaskRunner:
             (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Displays a prompt to users and asks capture permission for `mediaCaptureType`
// coming from a page with the given `origin`.
- (void)displayPromptForMediaCaptureType:(WKMediaCaptureType)mediaCaptureType
                                  origin:(const GURL&)origin;

// Displays a prompt to users and asks geolocation permission coming from a
// page with the given `origin`.
- (void)displayPromptForGeolocationOrigin:(const GURL&)origin;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_PERMISSION_REQUEST_H_
