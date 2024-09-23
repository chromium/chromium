// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_SYNC_INTERNALS_UI_H_
#define IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_SYNC_INTERNALS_UI_H_

#include <string>
#include <string_view>

#include "ios/components/webui/sync_internals/sync_internals_ui.h"

namespace web {
class WebUIIOS;
}  // namespace web

namespace ios_web_view {

// ios/web_view specific SyncInternalsUI.
class WebViewSyncInternalsUI : public SyncInternalsUI {
 public:
  WebViewSyncInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  WebViewSyncInternalsUI(const WebViewSyncInternalsUI&) = delete;
  WebViewSyncInternalsUI& operator=(const WebViewSyncInternalsUI&) = delete;

  ~WebViewSyncInternalsUI() override;
  bool OverrideHandleWebUIIOSMessage(const GURL& source_url,
                                     std::string_view message) override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEBUI_WEB_VIEW_SYNC_INTERNALS_UI_H_
