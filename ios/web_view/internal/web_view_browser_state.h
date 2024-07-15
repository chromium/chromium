// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_BROWSER_STATE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_BROWSER_STATE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_service.h"
#include "ios/web/public/browser_state.h"

namespace web {
class WebUIIOS;
}  // namespace web

namespace ios_web_view {

class WebViewURLRequestContextGetter;
class WebViewDownloadManager;

// WebView implementation of BrowserState. Can only be used only on the UI
// thread.
class WebViewBrowserState final : public web::BrowserState {
 public:
  explicit WebViewBrowserState(
      bool off_the_record,
      WebViewBrowserState* recording_browser_state = nullptr);

  WebViewBrowserState(const WebViewBrowserState&) = delete;
  WebViewBrowserState& operator=(const WebViewBrowserState&) = delete;

  ~WebViewBrowserState() override;

  // web::BrowserState implementation.
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;
  net::URLRequestContextGetter* GetRequestContext() override;

  // Returns the associated PrefService.
  PrefService* GetPrefs();

  // Returns the recording browser state associated with this browser state.
  // Returns |this| if called on a recording browser state.
  WebViewBrowserState* GetRecordingBrowserState();

  // Converts from web::BrowserState to WebViewBrowserState.
  static WebViewBrowserState* FromBrowserState(
      web::BrowserState* browser_state);

  static WebViewBrowserState* FromWebUIIOS(web::WebUIIOS* web_ui);

 private:
  // The path associated with this BrowserState object.
  base::FilePath path_;

  // Whether this BrowserState is incognito.
  bool off_the_record_;

  // The request context getter for this BrowserState object.
  scoped_refptr<WebViewURLRequestContextGetter> request_context_getter_;

  // The PrefService associated with this BrowserState.
  std::unique_ptr<PrefService> prefs_;

  // The recording browser state associated with this browser state.
  WebViewBrowserState* recording_browser_state_;

  // Handles browser downloads.
  std::unique_ptr<WebViewDownloadManager> download_manager_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_BROWSER_STATE_H_
