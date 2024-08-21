// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/webui/web_view_sync_internals_ui.h"

#import <string_view>

#import "components/sync/service/sync_internals_util.h"

namespace ios_web_view {

WebViewSyncInternalsUI::WebViewSyncInternalsUI(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : SyncInternalsUI(web_ui, host) {}

WebViewSyncInternalsUI::~WebViewSyncInternalsUI() {}

bool WebViewSyncInternalsUI::OverrideHandleWebUIIOSMessage(
    const GURL& source_url,
    std::string_view message) {
  // ios/web_view only supports sync in transport mode. Explicitly override the
  // sync start message and perform a no op.
  return message == syncer::sync_ui_util::kRequestStart;
}

}  // namespace ios_web_view
