// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import "build/blink_buildflags.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

// ContentWebFramesManager is a WebFramesManager that is built on top
// of //content.
class ContentWebFramesManager : public WebFramesManager {
 public:
  ContentWebFramesManager();
  ~ContentWebFramesManager() override;

  // WebFramesManager impl.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_
