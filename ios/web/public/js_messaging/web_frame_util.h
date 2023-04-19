// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_UTIL_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_UTIL_H_

#include <set>
#include <string>

namespace web {
class WebFrame;
class WebState;

// Returns the main WebFrame in `web_state`.
WebFrame* GetMainFrame(WebState* web_state);

// Returns the ID of `frame`. Returns std::string() if `frame` is nullptr.
std::string GetWebFrameId(WebFrame* frame);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAME_UTIL_H_
