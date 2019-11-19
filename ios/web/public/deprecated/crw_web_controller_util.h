// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_

@protocol CRWNativeContentProvider;
@protocol CRWSwipeRecognizerProvider;
@protocol CRWNativeContent;

namespace web {
class WebState;
}

// Utility functions needed to access CRWWebController methods via WebState.
// All functions defined here should be removed after native content removal and
// slim navigation launching.
namespace web_deprecated {

// Sets native provider for the |web_state|.
// |web_state| can't be null.
void SetNativeProvider(web::WebState* web_state,
                       id<CRWNativeContentProvider> delegate);

// Sets side swipe recognizer for the |web_state|.
// |web_state| can't be null.
void SetSwipeRecognizerProvider(web::WebState* web_state,
                                id<CRWSwipeRecognizerProvider> delegate);

// Gets the native controller associated with |web_state|.
// |web_state| can't be null.
id<CRWNativeContent> GetNativeController(web::WebState* web_state);

}  // namespace web_deprecated

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_
