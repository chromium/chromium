// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_CONTROLLER_H_
#define IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_CONTROLLER_H_

#include <string>
#include <string_view>

#import "base/memory/raw_ptr.h"

class GURL;

namespace web {

class WebUIIOS;

// A WebUIIOS page is controlled by the embedder's WebUIIOSController object. It
// manages the data source and message handlers.
class WebUIIOSController {
 public:
  WebUIIOSController(WebUIIOS* web_ui, const std::string& host)
      : web_ui_(web_ui), host_(host) {}
  virtual ~WebUIIOSController() {}

  // Allows the controller to override handling all messages from the page.
  // Return true if the message handling was overridden.
  virtual bool OverrideHandleWebUIIOSMessage(const GURL& source_url,
                                             std::string_view message);

  WebUIIOS* web_ui() const { return web_ui_; }

  // Gets the host associated with this WebUIIOSController.
  std::string GetHost() const { return host_; }

 private:
  raw_ptr<WebUIIOS> web_ui_;
  std::string host_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_CONTROLLER_H_
