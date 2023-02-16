// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import <map>

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;

class WebFramesManagerImpl : public WebFramesManager {
 public:
  // Returns the web frames manager for `web_state` and `content_world`.
  // `content_world` must specify a specific content world so
  // `kAllContentWorlds` is not a valid value.
  static WebFramesManagerImpl& FromWebState(web::WebState* web_state,
                                            ContentWorld content_world);

  WebFramesManagerImpl(const WebFramesManagerImpl&) = delete;
  WebFramesManagerImpl& operator=(const WebFramesManagerImpl&) = delete;

  ~WebFramesManagerImpl() override;

  // Adds `frame` to the list of web frames. A frame with the same frame ID must
  // not already be registered). Returns `false` and `frame` will be ignored if
  // `frame` is a main frame and a main frame has already been set.
  bool AddFrame(std::unique_ptr<WebFrame> frame);
  // Removes the web frame with `frame_id`, if one exists, from the list of
  // associated web frames. If the frame manager does not contain a frame with
  // `frame_id`, operation is a no-op.
  void RemoveFrameWithId(const std::string& frame_id);

  // WebFramesManager overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

 private:
  // Container that stores the web frame manager for each content world.
  // Usage example:
  //
  // WebFramesManagerImpl::Container::FromWebState(web_state)->
  //     ManagerForContentWorld(ContentWorld::kPageContentWorld);
  class Container : public web::WebStateUserData<Container> {
   public:
    ~Container() override;
    // Returns the web frames manager for `content_world`.
    WebFramesManagerImpl& ManagerForContentWorld(ContentWorld content_world);

   private:
    friend class web::WebStateUserData<Container>;
    WEB_STATE_USER_DATA_KEY_DECL();
    Container(web::WebState* web_state);

    web::WebState* web_state_ = nullptr;
    std::map<ContentWorld, std::unique_ptr<WebFramesManagerImpl>> managers_;
  };

  explicit WebFramesManagerImpl();

  // List of pointers to all web frames.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;

  // Reference to the current main web frame.
  WebFrame* main_web_frame_ = nullptr;
  base::ObserverList<Observer, /*check_empty=*/false> observers_;
  base::WeakPtrFactory<WebFramesManagerImpl> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
