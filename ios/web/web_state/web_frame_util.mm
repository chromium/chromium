// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/web_frame_util.h"

#include "base/logging.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frames_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
WebFrame* GetMainWebFrame(WebState* web_state) {
  WebFramesManager* manager = WebFramesManager::FromWebState(web_state);
  DCHECK(manager);
  return manager->GetMainWebFrame();
}

std::string GetMainWebFrameId(WebState* web_state) {
  WebFrame* main_frame = GetMainWebFrame(web_state);
  if (!main_frame) {
    return std::string();
  }
  return main_frame->GetFrameId();
}

WebFrame* GetWebFrameWithId(WebState* web_state, const std::string& frame_id) {
  if (frame_id.size() == 0)
    return nullptr;
  WebFramesManager* manager = WebFramesManager::FromWebState(web_state);
  DCHECK(manager);
  return manager->GetFrameWithId(frame_id);
}

std::string GetWebFrameId(WebFrame* frame) {
  return frame ? frame->GetFrameId() : std::string();
}

}  // namespace web
