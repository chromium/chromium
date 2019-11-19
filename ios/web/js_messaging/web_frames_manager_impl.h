// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_FRAMES_MANAGER_IMPL_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import <WebKit/WebKit.h>
#include <map>
#include "base/memory/weak_ptr.h"

@class CRWWKScriptMessageRouter;

namespace web {
class WebState;
class WebFrame;

// Delegate for WebFramesManager to publish frame events and get WebState.
class WebFramesManagerDelegate {
 public:
  virtual ~WebFramesManagerDelegate() {}

  // Will be invoked only once for each web frame in the page, when the frame is
  // loaded and a WebFrame is added to the WebFramesManager.
  virtual void OnWebFrameAvailable(WebFrame* frame) = 0;
  // Will be invoked only once for each web frame in the page, when the frame is
  // unloaed and before the WebFrame is removed from the WebFramesManager.
  virtual void OnWebFrameUnavailable(WebFrame* frame) = 0;

  virtual WebState* GetWebState() = 0;
};

class WebFramesManagerImpl : public WebFramesManager {
 public:
  explicit WebFramesManagerImpl(WebFramesManagerDelegate& delegate);
  ~WebFramesManagerImpl() override;

  // Removes all web frames from the list of associated web frames.
  void RemoveAllWebFrames();

  // Broadcasts a (not encrypted) JavaScript message to get the identifiers
  // and keys of existing frames.
  void RegisterExistingFrames();

  // Use |message_router| to unregister JS message handlers for |old_web_view|
  // and register handlers for |new_web_view|. Owner of this class should call
  // this method whenever associated WKWebView changes.
  void OnWebViewUpdated(WKWebView* old_web_view,
                        WKWebView* new_web_view,
                        CRWWKScriptMessageRouter* message_router);

  // WebFramesManager overrides.
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

 private:
  // Adds |frame| to the list of web frames associated with WebState and invoke
  // |delegate_|.OnWebFrameAvailable with |frame|. The frame must not be already
  // in the frame manager (the frame manager must not have a frame with the same
  // frame ID). If |frame| is a main frame, the frame manager must not have a
  // main frame already.
  void AddFrame(std::unique_ptr<WebFrame> frame);
  // Removes the web frame with |frame_id|, if one exists, from the list of
  // associated web frames, and invoke |delegate_|.OnWebFrameUnavailable with
  // the web frame. If the frame manager does not contain a frame with
  // |frame_id|, operation is a no-op.
  void RemoveFrameWithId(const std::string& frame_id);

  // Handles FrameBecameAvailable JS message and creates new WebFrame based on
  // frame info from the message (e.g. ID, encryption key, message counter,
  // etc.).
  void OnFrameBecameAvailable(WKScriptMessage* message);

  // Handles FrameBecameUnavailable JS message and removes the WebFrame with ID
  // from the message.
  void OnFrameBecameUnavailable(WKScriptMessage* message);

  // List of pointers to all web frames associated with WebState.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;

  // Reference to the current main web frame.
  WebFrame* main_web_frame_ = nullptr;

  // Reference to the delegate.
  WebFramesManagerDelegate& delegate_;

  base::WeakPtrFactory<WebFramesManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebFramesManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_FRAMES_MANAGER_H_
