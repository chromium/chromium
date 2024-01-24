// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_
#define IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_

#import "base/memory/raw_ptr.h"

namespace web {

class WebUIIOS;
class WebUIIOSImpl;

// Messages sent from the DOM are forwarded via the WebUIIOS to handler
// classes. These objects are owned by WebUIIOS and destroyed when the
// host is destroyed.
class WebUIIOSMessageHandler {
 public:
  WebUIIOSMessageHandler() : web_ui_(nullptr) {}
  virtual ~WebUIIOSMessageHandler() {}

 protected:
  // This is where subclasses specify which messages they'd like to handle and
  // perform any additional initialization. At this point web_ui() will return
  // the associated WebUIIOS object.
  virtual void RegisterMessages() = 0;

  // Returns the attached WebUIIOS for this handler.
  WebUIIOS* web_ui() const { return web_ui_; }

  // Sets the attached WebUIIOS - exposed to subclasses for testing purposes.
  void set_web_ui(WebUIIOS* web_ui) { web_ui_ = web_ui; }

 private:
  // Provide external classes access to web_ui() and set_web_ui().
  friend class WebUIIOSImpl;

  raw_ptr<WebUIIOS> web_ui_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_
