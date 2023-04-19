// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/web_frame_util.h"

#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebFrame* GetMainFrame(WebState* web_state) {
  return web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
}

std::string GetWebFrameId(WebFrame* frame) {
  return frame ? frame->GetFrameId() : std::string();
}

}  // namespace web
