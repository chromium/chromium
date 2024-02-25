// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAMES_MANAGER_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import <map>

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"

namespace web {
class WebFrame;

// A minimal implementation of web::WebFramesManager. Common usage pattern:
//   1. Create TestWebState;
//   2. Create FakeWebFramesManager;
//   3. Call TestWebState::SetWebFramesManager with FakeWebFramesManager;
//   4. Manipulate WebFrames:
//     a. Call FakeWebFramesManager::AddWebFrame
//     b. Call FakeWebFramesManager::RemoveWebFrame.
class FakeWebFramesManager : public WebFramesManager {
 public:
  FakeWebFramesManager();
  ~FakeWebFramesManager() override;

  // Adds and removes observers of WebFrame availability.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame);
  void RemoveWebFrame(const std::string& frame_id);

 protected:
  // List of pointers to all web frames associated with WebState.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;
  // Reference to the current main web frame.
  raw_ptr<WebFrame> main_web_frame_ = nullptr;
  base::ObserverList<Observer, /*check_empty=*/false> observers_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAMES_MANAGER_H_
