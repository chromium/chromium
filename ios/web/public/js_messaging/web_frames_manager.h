// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_

#include <set>
#include <string>

#include "base/macros.h"

namespace web {

class WebFrame;

// Stores and provides access to all WebFrame objects associated with a
// particular WebState.
// NOTE: Code that store references to WebFrames must clear them in
// WebStateObserver::WebFrameWillBecomeUnavailable, which is emitted when
// WebFrames in current page become invalid and will be removed from
// WebFramesManager (e.g. A new navigation is committed or failed, the web
// process crashed, etc.).
class WebFramesManager {
 public:
  virtual ~WebFramesManager() {}

  // Returns a list of all the web frames associated with WebState.
  // NOTE: Due to the asynchronous nature of renderer, this list may be
  // outdated.
  virtual std::set<WebFrame*> GetAllWebFrames() = 0;
  // Returns the web frame for the main frame associated with WebState or null
  // if unknown.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated.
  virtual WebFrame* GetMainWebFrame() = 0;
  // Returns the web frame with |frame_id|, if one exists.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated and the WebFrame returned by this method may
  // not back a real frame in the web page.
  virtual WebFrame* GetFrameWithId(const std::string& frame_id) = 0;

 protected:
  WebFramesManager() {}

  DISALLOW_COPY_AND_ASSIGN(WebFramesManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_
