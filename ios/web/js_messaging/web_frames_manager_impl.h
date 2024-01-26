// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import <map>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"

namespace web {
class WebFrame;

class WebFramesManagerImpl : public WebFramesManager {
 public:
  explicit WebFramesManagerImpl();
  ~WebFramesManagerImpl() override;

  WebFramesManagerImpl(const WebFramesManagerImpl&) = delete;
  WebFramesManagerImpl& operator=(const WebFramesManagerImpl&) = delete;

  // Adds `frame` to the list of web frames. A frame with the same frame ID must
  // not already be registered). Returns `false` and `frame` will be ignored if
  // `frame` is a main frame and a main frame has already been set.
  bool AddFrame(std::unique_ptr<WebFrame> frame);
  // Removes the web frame with `frame_id`, if one exists, from the list of
  // associated web frames. If the frame manager does not contain a frame with
  // `frame_id`, operation is a no-op.
  void RemoveFrameWithId(const std::string& frame_id);
  // Removes all the associated web frames.
  void RemoveAllWebFrames();

  // WebFramesManager overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

 private:
  // List of pointers to all web frames.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;

  // Reference to the current main web frame.
  raw_ptr<WebFrame> main_web_frame_ = nullptr;
  base::ObserverList<Observer, /*check_empty=*/false> observers_;
  base::WeakPtrFactory<WebFramesManagerImpl> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
