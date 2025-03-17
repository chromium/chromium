// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_

#import "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

class InternalDebugPagesDisabledUI : public web::WebUIIOSController {
 public:
  explicit InternalDebugPagesDisabledUI(web::WebUIIOS* web_ui,
                                        const std::string& host_name);

  InternalDebugPagesDisabledUI(const InternalDebugPagesDisabledUI&) = delete;
  InternalDebugPagesDisabledUI& operator=(const InternalDebugPagesDisabledUI&) =
      delete;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_
