// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOSDataSource;
}

class FlagsUI : public web::WebUIIOSController {
 public:
  explicit FlagsUI(web::WebUIIOS* web_ui);
  ~FlagsUI() override;
  static void AddFlagsIOSStrings(web::WebUIIOSDataSource* source);

 private:
  base::WeakPtrFactory<FlagsUI> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FlagsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
