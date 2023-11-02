// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
#define IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public web::WebUIIOSController {
 public:
  SyncInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  SyncInternalsUI(const SyncInternalsUI&) = delete;
  SyncInternalsUI& operator=(const SyncInternalsUI&) = delete;

  ~SyncInternalsUI() override;
};

#endif  // IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
