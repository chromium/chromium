// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_VIEW_PASS_KEY_H_
#define IOS_WEB_WEB_STATE_WEB_VIEW_PASS_KEY_H_

#import "base/types/pass_key.h"

namespace web {

class CobaltViewProvider;

// Grants access to WebStateImpl::GetWebView to access the WKWebView of the
// WebState if it exists.
//
// NOTE: Direct access to the WKWebView is strongly discouraged. It exposes
// global configurations that, if modified, can unexpectedly alter the
// behavior of the entire application. When specific functionality from the
// web view is needed, prefer adding a targeted proxy interface instead. The
// only acceptable use case for retrieving the raw WKWebView is when passing
// it to another WebKit API that explicitly requires the object instance.
using WebViewPassKey = base::PassKey<CobaltViewProvider>;

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_VIEW_PASS_KEY_H_
