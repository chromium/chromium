// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"

#import "base/base64.h"
#import "base/strings/utf_string_conversions.h"
#import "components/dom_distiller/ios/distiller_page_utils.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

ReaderModeDistillerPage::ReaderModeDistillerPage(web::WebState* web_state)
    : web_state_(web_state) {}
ReaderModeDistillerPage::~ReaderModeDistillerPage() = default;

void ReaderModeDistillerPage::DistillPageImpl(const GURL& url,
                                              const std::string& script) {
  if (!url.is_valid() || !script.length()) {
    return;
  }
  // Gets the instance of the WebFramesManager from `web_state_` that can
  // execute the DOM distiller JavaScript in the isolated content world.
  web::WebFramesManager* web_frames_manager =
      web_state_->GetWebFramesManager(web::ContentWorld::kIsolatedWorld);
  web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
  if (!main_frame) {
    return;
  }
  main_frame->ExecuteJavaScript(
      base::UTF8ToUTF16(script),
      base::BindOnce(&ReaderModeDistillerPage::HandleJavaScriptResult,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

bool ReaderModeDistillerPage::ShouldFetchOfflineData() {
  return false;
}

void ReaderModeDistillerPage::HandleJavaScriptResult(
    const GURL& url,
    const base::Value* result) {
  base::Value result_as_value =
      dom_distiller::ParseValueFromScriptResult(result);
  OnDistillationDone(url, &result_as_value);
}
