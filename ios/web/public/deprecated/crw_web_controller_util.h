// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_

@protocol CRWSwipeRecognizerProvider;

namespace web {
class WebState;
}

// Utility functions needed to access CRWWebController methods via WebState.
// All functions defined here should be removed after native content removal and
// slim navigation launching.
namespace web_deprecated {

// Sets side swipe recognizer for the `web_state`.
// `web_state` can't be null.
void SetSwipeRecognizerProvider(web::WebState* web_state,
                                id<CRWSwipeRecognizerProvider> delegate);

}  // namespace web_deprecated

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_WEB_CONTROLLER_UTIL_H_
