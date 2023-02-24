// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_web_frames_manager.h"

#import <set>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContentWebFramesManager::ContentWebFramesManager() {}

ContentWebFramesManager::~ContentWebFramesManager() {}

void ContentWebFramesManager::AddObserver(Observer* observer) {}

void ContentWebFramesManager::RemoveObserver(Observer* observer) {}

std::set<WebFrame*> ContentWebFramesManager::GetAllWebFrames() {
  return std::set<WebFrame*>();
}

WebFrame* ContentWebFramesManager::GetMainWebFrame() {
  return nullptr;
}

WebFrame* ContentWebFramesManager::GetFrameWithId(const std::string& frame_id) {
  return nullptr;
}

}  // namespace web
