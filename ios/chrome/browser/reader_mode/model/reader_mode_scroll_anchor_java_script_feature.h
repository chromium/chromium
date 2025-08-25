// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_SCROLL_ANCHOR_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_SCROLL_ANCHOR_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature that finds a scroll anchor in the page.
class ReaderModeScrollAnchorJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ReaderModeScrollAnchorJavaScriptFeature* GetInstance();

  // Finds the paragraph closest to the middle of the viewport.
  void FindScrollAnchor(web::WebFrame* web_frame);

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 private:
  friend class base::NoDestructor<ReaderModeScrollAnchorJavaScriptFeature>;

  ReaderModeScrollAnchorJavaScriptFeature();
  ~ReaderModeScrollAnchorJavaScriptFeature() override;

  ReaderModeScrollAnchorJavaScriptFeature(
      const ReaderModeScrollAnchorJavaScriptFeature&) = delete;
  ReaderModeScrollAnchorJavaScriptFeature& operator=(
      const ReaderModeScrollAnchorJavaScriptFeature&) = delete;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_SCROLL_ANCHOR_JAVA_SCRIPT_FEATURE_H_
