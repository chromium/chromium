// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_

#import <set>
#import <string>

#import "base/observer_list_types.h"

namespace web {

class WebFrame;

// Stores and provides access to all WebFrame objects associated with a
// particular WebState.
class WebFramesManager {
 public:
  // Observer class to notify objects when WebFrames are added or removed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when a frame is created or the user navigates to a new document.
    // Receivers can keep references to `web_frame` only until
    // `WebFrameBecameUnavailable` at which point the pointer will become
    // invalid.
    // TODO(crbug.com/40276017): This should pass a WeakPtr instead.
    virtual void WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                                         WebFrame* web_frame) {}

    // Called when a frame is deleted or the user navigates away from
    // `web_frame` before it is removed from the WebFramesManager. Receivers of
    // this callback must clear any stored references to the `web_frame` with
    // `frame_id`.
    virtual void WebFrameBecameUnavailable(WebFramesManager* web_frames_manager,
                                           const std::string& frame_id) {}
  };

  WebFramesManager(const WebFramesManager&) = delete;
  WebFramesManager& operator=(const WebFramesManager&) = delete;

  virtual ~WebFramesManager() {}

  // Adds and removes observers of WebFrame availability.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // TODO(crbug.com/40276017): Transition the below functions to return
  // WeakPtrs.

  // Returns a list of all the web frames associated with WebState.
  // NOTE: Due to the asynchronous nature of renderer, this list may be
  // outdated.
  virtual std::set<WebFrame*> GetAllWebFrames() = 0;
  // Returns the web frame for the main frame associated with WebState or null
  // if unknown.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated.
  virtual WebFrame* GetMainWebFrame() = 0;
  // Returns the web frame with `frame_id`, if one exists.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated and the WebFrame returned by this method may
  // not back a real frame in the web page.
  virtual WebFrame* GetFrameWithId(const std::string& frame_id) = 0;

 protected:
  WebFramesManager() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_FRAMES_MANAGER_H_
