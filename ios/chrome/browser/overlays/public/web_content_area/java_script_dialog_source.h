// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_

#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

namespace web {
class WebState;
}

// Object used to capture the source of a JavaScript dialog.
class JavaScriptDialogSource : public web::WebStateObserver {
 public:
  JavaScriptDialogSource(web::WebState* web_state,
                         const GURL& url,
                         bool is_main_frame);
  JavaScriptDialogSource(const JavaScriptDialogSource& source);
  ~JavaScriptDialogSource() override;

  // The WebState requesting the dialog.
  web::WebState* web_state() const { return web_state_; }
  // The URL of the page requesting the JavaScript dialog.
  const GURL& url() const { return url_; }
  // Whether or not the requesting page is in the main frame.
  bool is_main_frame() const { return is_main_frame_; }

 private:
  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  web::WebState* web_state_;
  GURL url_;
  bool is_main_frame_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOG_SOURCE_H_
